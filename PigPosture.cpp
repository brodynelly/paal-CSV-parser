#include "PigPosture.h"
#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/stream/document.hpp>


bsoncxx::document::value Posture::to_bson() const {
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;

    return document{}
        << "pigId" << pigId
        << "timestamp" << bsoncxx::types::b_date{timestamp}
        << "score" << score
        << finalize;
}
