#ifndef DB_DATA_TYPE_CATEGORY
#define DB_DATA_TYPE_CATEGORY


namespace meow {
namespace db {

enum DataTypeCategoryIndex {
    Integer,
    Float,// H: Real
    Text,
    Binary,
    Temporal,
    Spatial,
    Other
};


} // namespace db
} // namespace meow

#endif // DB_DATA_TYPE_CATEGORY

