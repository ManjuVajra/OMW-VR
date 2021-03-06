#ifndef OPENMW_ESM_COMMON_H
#define OPENMW_ESM_COMMON_H

#include <algorithm>
#include <string>
#include <cstring>
#include <vector>

#include <stdint.h>

namespace ESM
{
enum Version
  {
    VER_12 = 0x3f99999a,
    VER_13 = 0x3fa66666
  };

enum RecordFlag
  {
    FLAG_Persistent = 0x00000400,
    FLAG_Blocked    = 0x00002000
  };

// CRTP for FIXED_STRING class, a structure used for holding fixed-length strings
template< template<size_t> class DERIVED, size_t SIZE>
class FIXED_STRING_BASE
{
    /* The following methods must be implemented in derived classes:
     *   char const* ro_data() const; // return pointer to ro buffer
     *   char*       rw_data();       // return pointer to rw buffer
     */
public:
    enum { size = SIZE };

    template<size_t OTHER_SIZE>
    bool operator==(char const (&str)[OTHER_SIZE]) const
    {
        size_t other_len = strnlen(str, OTHER_SIZE);
        if (other_len != this->length())
            return false;
        return std::strncmp(self()->ro_data(), str, size) == 0;
    }

    //this operator will not be used for char[N], only for char*
    template<typename T, typename = typename std::enable_if<std::is_same<T, char>::value>::type>
    bool operator==(const T* const& str) const
    {
        char const* const data = self()->ro_data();
        for(size_t i = 0; i < size; ++i)
        {
            if(data[i] != str[i]) return false;
            else if(data[i] == '\0') return true;
        }
        return str[size] == '\0';
    }
    bool operator!=(const char* const str) const { return !( (*this) == str ); }

    bool operator==(const std::string& str) const
    {
        return (*this) == str.c_str();
    }
    bool operator!=(const std::string& str) const { return !( (*this) == str ); }

    static size_t data_size() { return size; }
    size_t length() const { return strnlen(self()->ro_data(), size); }
    std::string toString() const { return std::string(self()->ro_data(), this->length()); }

    void assign(const std::string& value)
    {
        std::strncpy(self()->rw_data(), value.c_str(), size-1);
        self()->rw_data()[size-1] = '\0';
    }

    void clear() { this->assign(""); }
private:
    DERIVED<size> const* self() const
    {
        return static_cast<DERIVED<size> const*>(this);
    }

    // write the non-const version in terms of the const version
    // Effective C++ 3rd ed., Item 3 (p. 24-25)
    DERIVED<size>* self()
    {
        return const_cast<DERIVED<size>*>(static_cast<FIXED_STRING_BASE const*>(this)->self());
    }
};

// Generic implementation
template <size_t SIZE>
struct FIXED_STRING : public FIXED_STRING_BASE<FIXED_STRING, SIZE>
{
    char data[SIZE];

    char const* ro_data() const { return data; }
    char*       rw_data() { return data; }
};

// In the case of SIZE=4, it can be more efficient to match the string
// as a 32 bit number, therefore the struct is implemented as a union with an int.
template <>
struct FIXED_STRING<4> : public FIXED_STRING_BASE<FIXED_STRING, 4>
{
    char data[4];

    using FIXED_STRING_BASE::operator==;
    using FIXED_STRING_BASE::operator!=;

    bool operator==(uint32_t v) const { return v == toInt(); }
    bool operator!=(uint32_t v) const { return v != toInt(); }

    FIXED_STRING<4>& operator=(std::uint32_t value)
    {
        std::memcpy(data, &value, sizeof(data));
        return *this;
    }

    void assign(const std::string& value)
    {
        std::memset(data, 0, sizeof(data));
        std::memcpy(data, value.data(), std::min(value.size(), sizeof(data)));
    }

    char const* ro_data() const { return data; }
    char*       rw_data() { return data; }

    std::uint32_t toInt() const
    {
        std::uint32_t value;
        std::memcpy(&value, data, sizeof(data));
        return value;
    }
};

typedef FIXED_STRING<4> NAME;
typedef FIXED_STRING<32> NAME32;
typedef FIXED_STRING<64> NAME64;

/* This struct defines a file 'context' which can be saved and later
   restored by an ESMReader instance. It will save the position within
   a file, and when restored will let you read from that position as
   if you never left it.
 */
struct ESM_Context
{
  std::string filename;
  uint32_t leftRec, leftSub;
  size_t leftFile;
  NAME recName, subName;
  // When working with multiple esX files, we will generate lists of all files that
  //  actually contribute to a specific cell. Therefore, we need to store the index
  //  of the file belonging to this contest. See CellStore::(list/load)refs for details.
  int index;
  std::vector<int> parentFileIndices;

  // True if subName has been read but not used.
  bool subCached;

  // File position. Only used for stored contexts, not regularly
  // updated within the reader itself.
  size_t filePos;
};

}

#endif
