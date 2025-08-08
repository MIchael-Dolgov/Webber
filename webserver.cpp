#include <iostream>
#include <unordered_map>
#include <filesystem>

#include "cli.cpp"
#include "sockets.cpp"
#include "httphelper.cpp"
#include "indexingtools.hpp"

#define FIRST_USER_FLAG 1

int main(int argc, char *argv[])
{
    using std::string;
    using HTTP::Request;
    using HTTP::Response;
    using HTTP::DeserializedHeader;
    //args initialization
    WebCliConfig &server_conf = WebCliConfig::instance();
    if(argc >= 2 && (string)argv[FIRST_USER_FLAG] == "--help")
    {
        std::cout << WebCliConfig::help() << "\n";
        return EXIT_SUCCESS;
    }
    for(int i = 1; i+1 <= argc; i+=2)
    {
        try
        {
            server_conf.setParam(argv[i], argv[i+1]);
        }
        catch(const std::exception& err)
        {
            std::cerr << "invalid parameters sequence: " << err.what() << "\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "Indexing all routes and mapping to files from file: " << 
        server_conf.getRoutesFile() << " " << "..." << "\n";
    std::unordered_map<std::string, std::string> router = {};
    try
    {
        //indexing all routes and link to their paths
        IndexingTools::FileExplorer routesExplorer = 
            IndexingTools::FileExplorer(server_conf.getRoutesFile());
        std::string output;
        IndexingTools::FileExplorer::FileExplorerIterator *iter = 
            routesExplorer.getIterator();
        while(iter->next(output))
        {
            int space_pos = output.find(' ');
            if (space_pos != std::string::npos) 
            {
                std::string first = output.substr(0, space_pos);
                std::string second = output.substr(space_pos + 1);
                router[first] = second;
            } 
            else 
            {
                std::cerr << "invalid route: " << output << "skipping line.";
            }
        }
        if (!iter->isEnd())
        {
            throw std::runtime_error("file reading went wrong");
        }
    }
    catch (const std::exception& err)
    {
        std::cerr << "\e[0;31m";
        std::cerr << "Indexing failed: " << err.what() << "\n";
        return EXIT_FAILURE;
    }
    //init all networking entities for http workflow
    sockets::SocketListener &main_web_gate = 
        sockets::SocketListener::instance();
    try 
    {
        main_web_gate.configure(server_conf.getMaxConns(), server_conf.getIP(),
            server_conf.getPort()); 
        main_web_gate.startListen(); 
    }
    catch(const std::exception& err)
    {
        std::cerr << "Failed to open listening socket: " << err.what() << "\n";
        return EXIT_FAILURE;
    }

    // Configure web server event loop
    int listener_fd = main_web_gate.GetListenerDescriptor();
    int fdmax = listener_fd;
    fd_set master, read_fds, write_fds, except_fds; //set for all descriptors
    //clear all
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    // add the listener to the master set
    FD_SET(listener_fd, &master);

    // web server clients
    int newfd_var; //newly accepted socket descriptor
    struct sockaddr_storage remoteaddr; //client address
    socklen_t addrlen; // sizeof new socket
    std::unordered_map<int, sockets::ClientSocketHandler*> client_sockets 
        = {};
    ssize_t nbytes; // client socket request size

    // I/O event loop
    int read_write_except_event_status;
    while(true) 
    {
        read_fds = master; 
        read_write_except_event_status = select(fdmax+1, &read_fds, 
            &write_fds, NULL, NULL);
        if(read_write_except_event_status == -1) 
        {
            std::cerr << "selector issue";
            return EXIT_FAILURE;
        }
        //check IO events
        //TODO: restrict CPU time against overload (clock lock at 1 min to 10% of CPU time)
        for(int i = 0; i <= fdmax; i++)
        {
            if (!FD_ISSET(i, &master))
            {
                continue;
            }
            // New TCP connection!
            if (i == listener_fd && FD_ISSET(i, &read_fds))
            {
                addrlen = sizeof(remoteaddr);
                newfd_var = accept(listener_fd,
                    (struct sockaddr *)&remoteaddr,
                    &addrlen);
                if (newfd_var == -1) 
                {
                    std::cerr << "New TCP connection failed. Refusing..." 
                        << "\n";
                    continue;
                }
                FD_SET(newfd_var, &master);
                if (newfd_var > fdmax)
                {
                    fdmax = newfd_var;
                }
                client_sockets[newfd_var] 
                    = new sockets::ClientSocketHandler(newfd_var,
                        (struct sockaddr&)remoteaddr);
                // new connection terminal log
                std::cout << "\e[1;36m";
                std::cout << "new connection from " << 
                    client_sockets[newfd_var]->getIP() << "\n";
                std::cout << "from port: " <<
                    client_sockets[newfd_var]->getPort() << "\n";
                std::cout << "with fd: " << 
                    client_sockets[newfd_var]->getFd() << "\n";
                std::cout << "\e[0;37m";
            }

            // Connected client is sending data
            else if (i != listener_fd && FD_ISSET(i, &read_fds))
            {

                nbytes = client_sockets[i]->proceedIncomeSocketDataThreaded();
                if(nbytes <= 0)
                {
                    if (nbytes == 0)
                    {
                        std::cout << "close connection: " 
                            << client_sockets[i]->getIP() << "\n";
                    }
                    else
                    {
                        std::cerr 
                            << "socket handling error. Connection refused: "
                            << client_sockets[i]->getIP() << ":"
                            << client_sockets[i]->getPort() << " fd:"
                            << client_sockets[i]->getFd() << "\n";
                    }
                    delete client_sockets[i];
                    client_sockets.erase(i);
                    FD_CLR(i, &master);
                    FD_CLR(i, &write_fds);
                    continue; //skip already have data check for preventing
                }
                //Already have data?
                if(client_sockets.count(i) > 0 && 
                    !(client_sockets[i]->isSocketEmpty()))
                {
                    //Next processing round
                    FD_SET(i, &write_fds);
                }
            }

            else if (i != listener_fd && FD_ISSET(i, &write_fds) && 
                !(client_sockets[i]->isSocketEmpty()))
            {
                //TODO: отрефакторить точку отправки Response. (мультиплексор)
                Response resp;
                try
                {
                    std::cout
                        << "client is ready to receive data: "
                        << client_sockets[i]->getIP() << ":"
                        << client_sockets[i]->getPort() << " fd:"
                        << client_sockets[i]->getFd() << "\n";

                    std::cout
                        << "client: " << client_sockets[i]->getIP() << ":"
                        << client_sockets[i]->getPort() 
                        << " response is sending..." << "\n";

                    std::string var = (client_sockets[i]->forwardExtractedData());
                    Request *var2 = new Request();
                    // is HTTP 1.1 request? No? Then skip
                    if (!var2->tryExtractHTML(var)) // extraction from buffer failed
                    {
                        client_sockets[i]->freeUpBufferSpace(var2->request_char_len);
                        continue;
                    }
                    else // extraction get good request
                    {
                        client_sockets[i]->freeUpBufferSpace(var2->request_char_len); 
                        DeserializedHeader *header = 
                            new DeserializedHeader(var2->header);
                        //===== Your response logic =====
                        if(header->method == "GET")
                        {
                            //TODO: реализовать поддержку отправки фото и css
                            if(router.count(header->path) > 0)
                            {
                                std::string route = router[header->path];
                            
                                if(IndexingTools::isTextFileFormat(route))
                                {
                                    //Prepare file
                                    IndexingTools::FileExplorer file = 
                                        IndexingTools::FileExplorer(
                                            router[header->path],
                                            IndexingTools::OpenMode::Text
                                        );

                                    //File iterator
                                    IndexingTools::FileExplorer::FileExplorerIterator *it =
                                        file.getIterator();

                                    //Form response
                                    resp = Response (
                                        HTTP::MetaInfo::StatusCode::OK, 
                                        file.getFileSizeInBytes(), 
                                        server_conf.getSendingPacketSize(),
                                        HTTP::MetaInfo::convertTextToContentType(
                                            IndexingTools::getFileExtension(
                                                router[header->path]
                                            )
                                        ),
                                        it
                                    );

                                    //send response
                                    //HTTP meta data
                                    client_sockets[i]->sendDataThreaded(
                                        resp.getHTTPmeta().c_str(),
                                        resp.getHTTPmeta().length()
                                    );
                                    //head + body
                                    std::string dataBite;
                                    while(resp.nextDataPiece(dataBite))
                                    {
                                        client_sockets[i]->sendDataThreaded(
                                            dataBite.c_str(),
                                            resp.getResponseSize()
                                        );
                                    }
                                }
                                //is not a text file
                                else
                                {
                                    IndexingTools::FileExplorer file = 
                                        IndexingTools::FileExplorer(
                                            router[header->path],
                                            IndexingTools::OpenMode::Binary
                                        );

                                    IndexingTools::FileExplorer::FileExplorerIterator *it =
                                        file.getIterator();

                                    resp = Response (
                                        HTTP::MetaInfo::StatusCode::OK,
                                        file.getFileSizeInBytes(),
                                        server_conf.getSendingPacketSize(),
                                        HTTP::MetaInfo::convertTextToContentType(
                                            IndexingTools::getFileExtension(
                                                router[header->path]
                                            )
                                        ),
                                        it
                                    );
                                    client_sockets[i]->sendDataThreaded(
                                        resp.getHTTPmeta().c_str(),
                                        resp.getHTTPmeta().length()
                                    );

                                    std::vector<char> buffer(
                                        server_conf.getSendingPacketSize());

                                    while(it->nextBytes(buffer, 
                                        server_conf.getSendingPacketSize()))
                                    {
                                        client_sockets[i]->sendDataThreaded(
                                            &buffer,
                                            buffer.size()
                                        );
                                    }
                                }
                            }
                            else
                            {
                                resp = Response (
                                    HTTP::MetaInfo::StatusCode::NotFound
                                );
                                client_sockets[i]->sendDataThreaded(
                                    resp.getHTTPmeta().c_str(),
                                    resp.getHTTPmeta().length()
                                );
                            }
                        }
                        else
                        {
                            resp = Response (
                                HTTP::MetaInfo::StatusCode::NotImplemented
                            );
                            client_sockets[i]->sendDataThreaded(
                                resp.getHTTPmeta().c_str(),
                                resp.getHTTPmeta().length()
                            );
                        }
                    }
                }
                catch(const std::exception& err) 
                {
                    std::cerr << "\e[0;31m";
                    std::cerr << "Internal server error: " 
                        << err.what() << "\n";

                    resp = Response (
                        HTTP::MetaInfo::StatusCode::InternalServerError
                    );
                    client_sockets[i]->sendDataThreaded(
                        resp.getHTTPmeta().c_str(),
                        resp.getHTTPmeta().length()
                    );
                }
                //End of response logic
                std::cout << "\e[0;33m";
                std::cout << "response has been send." << "\n";
                std::cout << "\e[0;37m";
            }
            // All client requests satisfied
            else if(FD_ISSET(i, &write_fds) && 
                client_sockets[i]->isSocketEmpty())
            {
                FD_CLR(i, &write_fds);
            }
        }
    }

    return EXIT_SUCCESS;
}
