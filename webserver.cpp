#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include "cli.cpp"
#include "sockets.cpp"
#include "httphelper.cpp"
#include "indexingtools.hpp"
#include "stringtools.hpp"

#define FIRST_USER_FLAG 1

int main(int argc, char *argv[])
{
    using std::string;
    using HTTP::Request;
    using HTTP::Response;
    using HTTP::DeserializedHeader;
    using CliTools::printColoredMessage;
    using CliTools::ConsoleColor;
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
            // ignore comments on routes page
            size_t pos = output.find('#');
            if(!(pos == std::string::npos)) {output.erase(pos);}
            trim(output);
            if(output.length() == 0) {continue;} //comment line in file
            int space_pos = output.find(' ');
            // Try parse by route rule
            if(space_pos == std::string::npos) 
            {
                std::cerr << "Space has not been found for: "
                    << output << " skipping line." << "\n";
            } 
            else 
            {
                std::string first = output.substr(0, space_pos);
                std::string second = output.substr(space_pos + 1);
                trim(first);
                trim(second);
                if(!first.empty() && countChar(first, ' ') == 0
                    && !second.empty() && countChar(second, ' ') == 0)
                {
                    router[first] = second;
                }
                else 
                {
                    std::cerr << "invalid route: " 
                        << output << " " << "skipping line." << "\n";
                }
            }
        }
        if (!iter->isEnd())
        {
            throw std::runtime_error("file reading went wrong");
        }
    }
    catch (const std::exception& err)
    {
        printColoredMessage(
            std::cout,
            "Indexing failed: " + std::string(err.what()) + "\n", 
            ConsoleColor::Color::Red
        );
        return EXIT_FAILURE;
    }
    //init all networking entities for http workflow
    sockets::SocketListener &main_web_gate = 
        sockets::SocketListener::instance();
    try 
    {
        main_web_gate.configure(server_conf.getMaxConns(), server_conf.getIP(),
            server_conf.getPort()); 
        if(main_web_gate.startListen())
        {
            printColoredMessage(
                std::cout, 
                std::string("=====   Success!   =====") + "\n" +
                std::string("Web server is listening port: ") +
                main_web_gate.getListenerPort() + " " +
                std::string("On ip: ") + main_web_gate.getListenerHost() + "\n" +
                std::string("========================"),
                ConsoleColor::Color::Green
            );
        }
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
            printColoredMessage(
                std::cout,
                "selector issue", 
                ConsoleColor::Color::Red
            );
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
                    printColoredMessage(
                        std::cout,
                        "New TCP connection failed. Refusing...", 
                        ConsoleColor::Color::Yellow
                    );
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
                printColoredMessage(
                    std::cout, 
                    "new connection from " +
                    client_sockets[newfd_var]->getIP() + "\n" + 
                    "Port: " + 
                    std::to_string(client_sockets[newfd_var]->getPort()) + "\n" +
                    "Fd: " +
                    std::to_string(client_sockets[newfd_var]->getFd()), 
                    ConsoleColor::Color::Blue
                );
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
                client_sockets[i]->refreshLastSocketConversation();
            }
            
            else if (i != listener_fd && FD_ISSET(i, &write_fds) && 
                !(client_sockets[i]->isSocketEmpty()))
            {
                //TODO: отрефакторить точку отправки Response.
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
                            if(router.count(header->path) > 0)
                            {
                                std::string route = router[header->path];
                            
                                if(IndexingTools::isTextFileFormat(route))
                                {
                                    //Prepare file
                                    IndexingTools::FileExplorer file = 
                                        IndexingTools::FileExplorer(
                                            route,
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
                                                route
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
                                            route,
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
                                                route
                                            )
                                        ),
                                        it
                                    );
                                    client_sockets[i]->sendDataThreaded(
                                        resp.getHTTPmeta().c_str(),
                                        resp.getHTTPmeta().length()
                                    );

                                    try
                                    {
                                    std::vector<char> buffer(
                                        server_conf.getSendingPacketSize());

                                    while (it->nextBytes(buffer, buffer.size()))
                                        {
                                            client_sockets[i]->sendDataThreaded(
                                                buffer.data(),
                                                buffer.size()
                                            );
                                        }
                                    }
                                    catch(const std::exception& err)
                                    {
                                        printColoredMessage(
                                            std::cerr,
                                            std::string(err.what()), 
                                            ConsoleColor::Color::Yellow
                                        );

                                        resp = Response (
                                            HTTP::MetaInfo::
                                                StatusCode::InternalServerError
                                        );

                                        client_sockets[i]->sendDataThreaded(
                                            resp.getHTTPmeta().c_str(),
                                            resp.getHTTPmeta().length()
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
                    printColoredMessage(
                        std::cout, 
                        "Internal server error: " + std::string(err.what()), 
                        ConsoleColor::Color::Red
                    );

                    resp = Response (
                        HTTP::MetaInfo::StatusCode::InternalServerError
                    );
                    client_sockets[i]->sendDataThreaded(
                        resp.getHTTPmeta().c_str(),
                        resp.getHTTPmeta().length()
                    );
                }
                //End of response logic
                printColoredMessage(
                    std::cout, 
                    "response has been sent.", 
                    ConsoleColor::Color::Green
                );
                client_sockets[i]->refreshLastSocketConversation();
            }
            // All client requests satisfied
            else if(FD_ISSET(i, &write_fds) && 
                client_sockets[i]->isSocketEmpty())
            {
                FD_CLR(i, &write_fds);
            }
            //closing connection for abandoned sockets
            /*
            if(i != listener_fd && FD_ISSET(i, &master))
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - client_sockets[i]->getLastConversation()
                ).count();
                if(elapsed > server_conf.getAbandonedSocketTimeout())
                {
                    printColoredMessage(
                        std::cout, 
                        "Connection closed " +
                        client_sockets[newfd_var]->getIP() + "\n" + 
                        std::to_string(client_sockets[i]->getPort()) + "\n" +
                        std::to_string(client_sockets[i]->getFd()), 
                        ConsoleColor::Color::Yellow
                    ); 
                    delete client_sockets[i];
                    client_sockets.erase(i);
                    FD_CLR(i, &master);
                    FD_CLR(i, &write_fds);
                }
            }
            */
        }
    }

    return EXIT_SUCCESS;
}
