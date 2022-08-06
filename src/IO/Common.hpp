#ifndef IO_COMMON_HPP
#define IO_COMMON_HPP

#include <IO/CommonConcepts.hpp>
#include <IO/ByteBuffer.hpp>
#include <Utils/Meta/Traits.hpp>
#include <Utils/Meta/Templates.hpp>
#include <Validation/Contracts.hpp>
#include <Validation/Log.hpp>
#include <Config/CodeZones.hpp>

#include <unordered_map>
#include <fstream>
#include <type_traits>
#include <concepts>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <limits>

namespace IO::Common
{

  enum class FourCCEndian
  {
    LITTLE = 0, ///> commonly used order of chars in bytes (little endian, chars sre written right to left in the file).
    BIG = 1 ///> used in m2 (big endian, chars are written left to right in the file).
  };

  /**
   * Converts string representation of FourCC to integer in compile time.
   * @tparam fourcc FourCC identifier as string literal.
   * @tparam endian Determines endianness of the FourCC idetnifier.
   */
  template <Utils::Meta::Templates::StringLiteral fourcc, FourCCEndian endian = FourCCEndian::LITTLE>
  static constexpr std::uint32_t FourCC = static_cast<bool>(endian) ? (fourcc.value[3] << 24 | fourcc.value[2] << 16
       | fourcc.value[1] << 8 | fourcc.value[0])
       : (fourcc.value[0] << 24 | fourcc.value[1] << 16 | fourcc.value[2] << 8 | fourcc.value[3]);

  /**
   * Converts integer representation of FourCC to string at compile time.
   * @tparam fourcc_int FourCC identifier as integer.
   * @tparam endian Determines endianness of the FourCC idenitifer.
   */
  template <std::uint32_t fourcc_int, FourCCEndian endian = FourCCEndian::LITTLE>
  static constexpr char FourCCStr[5] = { static_cast<bool>(endian) ? fourcc_int & 0xFF : (fourcc_int >> 24) & 0xFF,
                                         static_cast<bool>(endian) ? (fourcc_int >> 8) & 0xFF
                                          : (fourcc_int >> 16) & 0xFF,
                                         static_cast<bool>(endian) ? (fourcc_int >> 16) & 0xFF
                                          : (fourcc_int >> 8) & 0xFF,
                                         static_cast<bool>(endian) ? (fourcc_int >> 24) & 0xFF : fourcc_int & 0xFF,
                                         '\0'
                                       };

  /**
   * Converts integer representation of FourCC to string at runtime.
   * @param fourcc FourCC identifier as integer.
   * @param is_big_endian Determines endiannes of the FourCC identifier.
   */
  inline std::string FourCCToStr(std::uint32_t fourcc, bool is_big_endian = false)
  {
    char fourcc_str[5] = { static_cast<char>(is_big_endian ? fourcc & 0xFF : (fourcc >> 24) & 0xFF),
                           static_cast<char>(is_big_endian ? (fourcc >> 8) & 0xFF : (fourcc >> 16) & 0xFF),
                           static_cast<char>(is_big_endian ? (fourcc >> 16) & 0xFF : (fourcc >> 8) & 0xFF),
                           static_cast<char>(is_big_endian ? (fourcc >> 24) & 0xFF : fourcc & 0xFF),
                           '\0'
                         };

    return {&fourcc_str[0]};
  }

  /**
   * Controls version of the client to be assumed when working with fileformat related classes.
   * Remastered (new classic) clients are positioned next to their closest relative retail client version.
   */
  enum class ClientVersion
  {
    CLASSIC = 0,
    TBC = 10,
    WOTLK = 20,
    CATA = 30,
    MOP = 40,
    WOD = 50,
    LEGION = 60,
    BFA = 70,
    SL = 80,
    DF = 90,

    // Classic era remasters
    CLASSIC_NEW = 71,
    TBC_NEW = 81,
    WOTLK_NEW = 91, // TODO: check if is actually based on Dragonflight

    ANY = 100000 ///> Indicates a feature currently present and not removed after the latest expansion.
  };

  /**
   * Represents a variety of the client localization options. A superset for all versions.
   */
  enum class ClientLocale
  {
    enGB = 0,
    enUS = 1,
    deDE = 2,
    koKR = 3,
    frFR = 4,
    zhCN = 5,
    zhTW = 6,
    esES = 7,
    esMX = 8,
    ruRU = 9,
    AUTO = 10
  };

  inline constexpr std::array<std::string_view, 10> ClientLocaleStr
  { "enGB", "enUS", "deDE", "koKR", "frFR", "zhCN", "zhTW", "esES", "esMX", "ruRU" };

  /**
   *   Each file chunk starts with this control structure.
   */
  struct ChunkHeader
  {
    std::uint32_t fourcc; ///> FourCC-magic idenitying the chunk.
    std::uint32_t size; ///> Size of chunk data in bytes.
  };

  /**
     DataChunk represents a common pattern within WoW files when a chunk contains
     exactly one element of underlying structure T, when header.size == sizeof(T).
     Example: ADT::MHDR and other header-like chunks.
     Convinience conversion operators are provided to cast it to the underlying T.

     @tparam T Data struxture that this chunk holds.
     @tparam fourcc FourCC identifier of the chunk.
     @tparam fourcc_endian FourCC identifier endianness.
   */
  template<Utils::Meta::Concepts::PODType T // Data structure that this chunk holds
      , std::uint32_t fourcc // FourCC identifier
      , FourCCEndian fourcc_endian = FourCCEndian::LITTLE> // FourCC endianness
  struct DataChunk
  {
    using ValueType = T;
    typedef std::conditional_t<sizeof(T) <= sizeof(std::size_t), T, T const&> InterfaceType;

    DataChunk() = default;

    /**
     * Contruct and initialize the data chunk with an existing structure (copy is made).
     * @param data_block Underlying structure.
     */
    explicit DataChunk(InterfaceType data_block);

    /**
     * Initialize the data chunk (underlying structure is default constructed).
     */
    void Initialize();

    /**
     * Initialize the data chunk with an existing structure (copy is made).
     * @param data_block Underlying structure.
     */
    void Initialize(InterfaceType data_block);

    /**
     * Read the data chunk from ByteBuffer.
     * @param buf ByteBuffer to read data from.
     * @param size Number of bytes to read from the buffer.
     */
    void Read(ByteBuffer const& buf, std::size_t size);

    /**
     * Write the data chunk into a ByteBuffer.
     * @param buf Self-owned ByteBuffer instance to write data into.
     */
    void Write(ByteBuffer& buf) const;

    /**
     * Size of the data chunk in bytes that it would take when written to file.
     * @return Size of the data chunk.
     */
    [[nodiscard]]
    std::size_t ByteSize() const { return sizeof(T); };

    /**
     * Chunk intiailization status.
     * @return True if chunk was initialized (existing within a file and containing valid data).
     */
    [[nodiscard]]
    bool IsInitialized() const { return _is_initialized; };

    // These operators are intended for supporting convenient conversions to underlying type
    [[nodiscard]]
    operator T&() { return data; };

    [[nodiscard]]
    operator T const&() const { return data; };

    [[nodiscard]]
    operator T*() { return &data; };

    [[nodiscard]]
    operator const T*() const {return &data; };

    [[nodiscard]]
    operator T() const { return data; };

    T data; ///> Underlying data structure.
    static constexpr std::uint32_t magic = fourcc;

  private:
    bool _is_initialized = false;
  };

  // Interface validity check
  static_assert(Concepts::DataChunkProtocol<DataChunk<std::uint32_t, 1>>);

  /**
   * DataArrayChunk represents a common pattern within WoW files where
   * a file chunk holds header.size / sizeof(T) instances of T.
   * Size constraints are provided to validate and control the max and min
   * number of elements in the underlying container, when it is required
   * semantically by the file format.
   *
   * If both size_min and size_max are the same, the chunk array is optimized
   * as a std::array with fixed number of elements, except when both
   * are std::numeric_limits<std::size_t>::max() (default). In that case, the array is
   * unconstrained (dynamic).
   *
   * The array implements an interface simialr to stl containers except providing
   * no exceptions. All validation is performed with contracts in debug mode.
   * @tparam T Data structure that this chunk holds.
   * @tparam fourcc FourCC identifier of the chunk.
   * @tparam fourcc_endian Determines endianness of the FourCC identifier.
   * @tparam size_min Minimum amount of elements stored in the array. std::size_t::max means a variable bound.
   * @tparam size_max Maximum amount of elements stored in the array. std::size_t::max means a variable bound.
   */
  template
  <
    Utils::Meta::Concepts::PODType T
    , std::uint32_t fourcc
    , FourCCEndian fourcc_endian = FourCCEndian::LITTLE
    , std::size_t size_min = std::numeric_limits<std::size_t>::max()
    , std::size_t size_max = std::numeric_limits<std::size_t>::max()
  >
  struct DataArrayChunk
  {
    using ValueType = T;
    using ArrayImplT = std::conditional_t<size_max == size_min && size_max < std::numeric_limits<std::size_t>::max()
      , std::array<T, size_max>, std::vector<T>>;
    using iterator = typename ArrayImplT::iterator;
    using const_iterator = typename ArrayImplT::const_iterator;

    /**
     * Initialize an empty array chunk
     */
    void Initialize();

   /**
    * Initialize the array chunk with n copies of underlying type T.
    * @param data_block Underlying stucture.
    * @param n Number of copies to make.
    */
    void Initialize(T const& data_block, std::size_t n);

    /**
     * Initialize the array chunk with existing array of underlying type T (std::vector or std::array).
     * @param data_array Array of underlying structures.
     */
    void Initialize(ArrayImplT const& data_array);

    /**
     * Read the array cunk from ByteBuffer (also initializes it).
     * @param buf ByteBuffer instance to read data from.
     * @param size Number of bytes to read from ByteBuffer.
     */
    void Read(ByteBuffer const& buf, std::size_t size);

    /**
     * Write contents of the array chunk into ByteBuffer.
     * @param buf Self-owned ByteBuffer instance to write data into.
     */
    void Write(ByteBuffer& buf) const;

    /**
     * Returns true if chunk is initialized (has valid data and is present in file).
     * @return true if chunk is initialized, else false.
     */
    [[nodiscard]]
    bool IsInitialized() const { return _is_initialized; };

    /**
     * Returns the number of elements stored in the container.
     * @return Number of elements in the container.
     */
    [[nodiscard]]
    std::size_t Size() const { return _data.size(); };

   /**
    * Returns the number of bytes that this array chunk would take in a file.
    * @return Number of bytes.
    */
    [[nodiscard]]
    std::size_t ByteSize() const { return _data.size() * sizeof(T); };

    /**
     * Default constructs a new element in the underlying vector and returns reference to it (dynamic size only).
     * @return Reference to the constructed object.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    T& Add() requires (std::is_same_v<ArrayImplT_, std::vector<T>>);

    /**
     * Removes an element by its index in the underlying vector. Bounds checks are debug-only, no exceptions.
     * (dynamic arrays only).
     * @param index
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    void Remove(std::size_t index) requires (std::is_same_v<ArrayImplT_, std::vector<T>>);

    /**
     * Removes an element by its iterator in the underlying vector. Bounds checks are debug-only, no exceptions.
     * (dynamic arrays only).
     * @param it Iterator pointing to the element to remove.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    void Remove(typename ArrayImplT_::iterator it) requires (std::is_same_v<ArrayImplT_, std::vector<T>>);

    /**
     *  Clears the underlying vector (dynamic size only).
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    void Clear() requires (std::is_same_v<ArrayImplT_, std::vector<T>>);

    /**
     * Returns reference to the element of the underlying vector by its index.
     * @param index Index of the elemnt in the underlying vector in a valid range.
     * @return Reference to vector element.
     */
    [[nodiscard]]
    T& At(std::size_t index);

    /**
     * Returns a const reference to the element of the underlying vector by its index.
     * @param index Index of the elemnt in the underlying vector in a valid range.
     * @return Constant reference to vector element.
     */
    [[nodiscard]]
    T const& At(std::size_t index) const;

    [[nodiscard]]
    typename ArrayImplT::const_iterator begin() const { return _data.cbegin(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator end() const { return _data.cend(); };

    [[nodiscard]]
    typename ArrayImplT::iterator begin() { return _data.begin(); };

    [[nodiscard]]
    typename ArrayImplT::iterator end() { return _data.end(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator cbegin() const { return _data.cbegin(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator cend() const { return _data.cend(); };

    [[nodiscard]]
    T& operator[](std::size_t index);

    [[nodiscard]]
    T const& operator[](std::size_t index) const;

    static constexpr std::uint32_t magic = fourcc;

  private:
    // Array of data structures (either std::vector or std::array depending on size constraints).
    ArrayImplT _data;

    bool _is_initialized = false;
  };

  // Interface validity checks
  static_assert(Concepts::DataArrayChunkProtocol<DataArrayChunk<std::uint32_t, 1>>);
  static_assert(Concepts::DataArrayChunkProtocol<DataArrayChunk<std::uint32_t, 1, FourCCEndian::LITTLE, 2, 2>>);

  /* StringBlockChunk represents a common pattern within WoW files where a chunk is an
     array of 0-terminated strings. It provides similar interface and options to DataArrayChunk.
   */

  enum class StringBlockChunkType
  {
    NORMAL = 0, ///> Simple array of null-terminated strings
    OFFSET = 1 ///> Offset map of null-terminated strings
  };

  /**
   * StringBlockChunk represents a common pattern within WoW files where a chunk is an array of 0-terminated strings.
   * It provides similar interfaces and options to DataArrayChunk.
   * @tparam type Type of the StringBlockChunk implementation.
   * @tparam fourcc FourCC identifier.
   * @tparam fourcc_endian Determines endianness of the FourCC identifier.
   * @tparam size_min Minimum amount of strings stored in the array. std::size_t::max means a variable bound.
   * @tparam size_max Maximum amount of strings store in the array. std::size_t::max means a variable bound.
   */
  template
  <
    StringBlockChunkType type
    , std::uint32_t fourcc
    , FourCCEndian fourcc_endian = FourCCEndian::LITTLE
    , std::size_t size_min = std::numeric_limits<std::size_t>::max()
    , std::size_t size_max = std::numeric_limits<std::size_t>::max()
  >
  struct StringBlockChunk
  {
    using ArrayImplT = std::conditional_t<type == StringBlockChunkType::NORMAL, std::vector<std::string>
        , std::vector<std::pair<std::uint32_t, std::string>>>;

    StringBlockChunk() = default;

    void Initialize();

    void Initialize(std::vector<std::string> const& strings) requires (type == StringBlockChunkType::NORMAL);
    void Initialize(std::vector<std::string> const& strings) requires (type == StringBlockChunkType::OFFSET);

    void Read(ByteBuffer const& buf, std::size_t size) requires (type == StringBlockChunkType::NORMAL);
    void Read(ByteBuffer const& buf, std::size_t size) requires (type == StringBlockChunkType::OFFSET);

    void Write(ByteBuffer& buf) const;

    /**
     * Returns true if chunk is initialized (has valid data and is present in file).
     * @return true if chunk is initialized, else false.
     */
    [[nodiscard]]
    bool IsInitialized() const { return _is_initialized; };

    /**
      * Returns the number of elements stored in the container.
      * @return Number of elements stored in the container.
      */
    [[nodiscard]]
    std::size_t Size() const { return _data.size(); };

    /**
    * Returns the number of bytes that this array chunk would take in a file.
    * @return Number of bytes.
    */
    [[nodiscard]]
    std::size_t ByteSize() const;

    /**
     * Pushes a string to the end of the underlying vector.
     * Uniqueness for the offset map variant is ensured.
     * @param string Any string. Note: for filepaths there is no game-format conversion performed.
     */
    void Add(std::string const& string);

    /**
     * Removes an element by its index in the underlying vector.
     * Bounds checks are debug-only, no exceptions.
     * @param index Index of an element to remove in a valid range.
     */
    void Remove(std::size_t index);

    /**
     * Removes an element by its iterator in the underlying vector.
     * Bounds checks are debug-only, no exceptions.
     * @param it Iterator pointing to an existing element.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    void Remove(typename ArrayImplT_::iterator it);

    /**
     * Removes and element by its const iterator in the underlying vector.
     * Bounds checks are debug-only, no exceptions.
     * @param it Const iterator pointing to an existing element.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    void Remove(typename ArrayImplT_::const_iterator it);

    /**
     * Clears the underlying vector.
     */
    void Clear();

    /**
     * Returns constant reference to the element of the underlying vector by its index.
     * Bounds checks are debug-only. No difference to [] operator.
     * @param index Index of an element in a valid range.
     * @return Constant reference to an element.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    [[nodiscard]]
    typename ArrayImplT_::value_type const& At(std::size_t index) const;

    /**
     * Returns reference to the element of the underlying vector by its index.
     * Bounds checks are debug-only. No difference to [] operator.
     * @param index Index of an element in a valid range.
     * @return Reference to an element.
     */
    template<typename..., typename ArrayImplT_ = ArrayImplT>
    [[nodiscard]]
    typename ArrayImplT_::value_type& At(std::size_t index);

    [[nodiscard]]
    typename ArrayImplT::const_iterator begin() const { return _data.cbegin(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator end() const { return _data.cend(); };

    [[nodiscard]]
    typename ArrayImplT::iterator begin() { return _data.begin(); };

    [[nodiscard]]
    typename ArrayImplT::iterator end() { return _data.end(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator cbegin() const { return _data.cbegin(); };

    [[nodiscard]]
    typename ArrayImplT::const_iterator cend() const { return _data.cend(); };

    template<typename..., typename ArrayImplT_ = ArrayImplT>
    [[nodiscard]]
    typename ArrayImplT_::value_type const& operator[](std::size_t index) const;

    template<typename..., typename ArrayImplT_ = ArrayImplT>
    [[nodiscard]]
    typename ArrayImplT_::value_type& operator[](std::size_t index);

    static constexpr std::uint32_t magic = fourcc;

  private:
    bool _is_initialized = false;
    ArrayImplT _data;
  };

  // Interface validity checks
  static_assert(Concepts::StringBlockChunkProtocol<StringBlockChunk<StringBlockChunkType::NORMAL, 1>>);
  static_assert(Concepts::StringBlockChunkProtocol<StringBlockChunk<StringBlockChunkType::OFFSET, 0>>);

}
#include <IO/Common.inl>
#endif // IO_COMMON_HPP