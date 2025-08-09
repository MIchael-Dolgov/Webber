#pragma once

#include <string>
#include <memory>

class IIterator {
public:
    virtual ~IIterator() = default;

    virtual bool next(std::string& outLine) = 0;

    virtual bool isEnd() const = 0;
};
