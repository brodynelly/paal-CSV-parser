#include "Pig.h"
#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/stream/document.hpp>

bsoncxx::document::value Pig::to_bson() const {
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;

    return document{}
        << "pigId" << pigId
        << "tag" << tag
        << "breed" << breed
        << "age" << age
        << "currentLocation" << bsoncxx::builder::stream::open_document
            << "farmId" << currentLocation.farmId
            << "barnId" << currentLocation.barnId
            << "stallId" << currentLocation.stallId
        << bsoncxx::builder::stream::close_document
        << "active" << true
        << "createdAt" << bsoncxx::types::b_date(std::chrono::system_clock::now())
        <<  "updatedAt" << bsoncxx::types::b_date(std::chrono::system_clock::now())
        << "__v" << 0
        << finalize;
}
