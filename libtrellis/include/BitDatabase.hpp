#ifndef LIBTRELLIS_BITDATABASE_HPP
#define LIBTRELLIS_BITDATABASE_HPP

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <boost/optional.hpp>
#include <mutex>
#include <boost/thread/shared_mutex.hpp>
#include <atomic>
#include <set>
#include <unordered_set>
#include "Util.hpp"

using namespace std;
namespace Trellis {
/*
The BitDatabase keeps track of what each bit in a tile does. Unlike other databases, this database is mutable from
 within libtrellis, for use during fuzzing.
 */

// A single configuration bit, given by its offset inside the tile,
// and whether or not it is inverted
struct ConfigBit {
    int frame;
    int bit;
    bool inv = false;

    inline bool operator==(const ConfigBit &other) const {
        return (frame == other.frame) && (bit == other.bit) && (inv == other.inv);
    }
};
}

namespace std {
// Hash function for ConfigBit
template<>
struct hash<Trellis::ConfigBit> {
public:
    inline size_t operator()(const Trellis::ConfigBit &bit) const {
        hash<int> hash_i_fn;
        hash<bool> hash_b_fn;
        return hash_i_fn(bit.frame) + hash_i_fn(bit.bit) + hash_b_fn(bit.inv);
    }
};
}

namespace Trellis {
typedef unordered_set<ConfigBit> BitSet;

// Write a configuration bit to string
inline string to_string(ConfigBit b) {
    ostringstream ss;
    if (b.inv) ss << "!";
    ss << "F" << b.frame;
    ss << "B" << b.bit;
    return ss.str();
}

// Read a configuration bit from a string
ConfigBit cbit_from_str(const string &s);

class CRAMView;

// A BitGroup is a list of configuration bits that correspond to a given setting
struct BitGroup {
    vector<ConfigBit> bits;

    // Return true if the BitGroup is set in a tile
    bool match(const CRAMView &tile) const;

    // Update a coverage set with the bitgroup
    void add_coverage(BitSet &known_bits) const;

    // Set the BitGroup in a tile
    void set_group(CRAMView &tile) const;

    // Clear the BitGroup in a tile
    void clear_group(CRAMView &tile) const;

    inline bool operator==(const BitGroup &other) const {
        return bits == other.bits;
    }
};

// Write BitGroup to output
ostream &operator<<(ostream &out, const BitGroup &bits);

// Read a BitGroup from input (until end of line)
istream &operator>>(istream &out, BitGroup &bits);

// An arc is a configurable connection between two nodes, defined within a mux
struct ArcData {
    string source;
    string sink;
    BitGroup bits;

    inline bool operator==(const ArcData &other) const {
        return (source == other.source) && (sink == other.sink) && (bits == other.bits);
    }
};

// A mux specifies all the possible source node arcs driving a sink node
struct MuxBits {
    string sink;
    vector<ArcData> arcs;

    // Work out which connection inside the mux, if any, is made inside a tile
    boost::optional<string>
    get_driver(const CRAMView &tile, boost::optional<BitSet &> coverage = boost::optional<BitSet &>()) const;

    // Set the driver to a given value inside the tile
    void set_driver(CRAMView &tile, const string &driver) const;

    inline bool operator==(const MuxBits &other) const {
        return (sink == other.sink) && (arcs == other.arcs);
    }
};

// Write mux database entry to output
ostream &operator<<(ostream &out, const MuxBits &mux);

// Read mux database entry (excluding .mux token) from input
istream &operator>>(istream &in, MuxBits &mux);

// There are three types of non-routing config setting in the database
// word  : a multibit setting, such as LUT initialisation
// simple: a single on/off setting, a special case of the above
// enum  : a setting with several different textual values, such as an IO type

struct WordSettingBits {
    string name;
    vector<BitGroup> bits;
    vector<bool> defval;

    // Return the word value in a tile, returning empty if equal to the default
    boost::optional<vector<bool>>
    get_value(const CRAMView &tile, boost::optional<BitSet &> coverage = boost::optional<BitSet &>()) const;

    // Set the word value in a tile
    void set_value(CRAMView &tile, const vector<bool> &value) const;

    inline bool operator==(const WordSettingBits &other) const {
        return (name == other.name) && (bits == other.bits) && (defval == other.defval);
    }
};

// Write config word setting bits to output
ostream &operator<<(ostream &out, const WordSettingBits &ws);

// Read config word database entry (excluding .config token) from input
istream &operator>>(istream &out, WordSettingBits &ws);

struct EnumSettingBits {
    string name;
    map<string, BitGroup> options;
    boost::optional<string> defval;

    // Get the value of the enumeration, returning empty if not set or set to default, if default is non-empty
    boost::optional<string>
    get_value(const CRAMView &tile, boost::optional<BitSet &> coverage = boost::optional<BitSet &>()) const;

    // Set the value of the enumeration in a tile
    void set_value(CRAMView &tile, const string &value) const;

    inline bool operator==(const EnumSettingBits &other) const {
        return (name == other.name) && (options == other.options) && (defval == other.defval);
    }
};

// Write config enum bits to output
ostream &operator<<(ostream &out, const EnumSettingBits &es);

// Read config enum bits database entry (excluding .config_enum token) from input
istream &operator>>(istream &out, EnumSettingBits &es);

struct TileConfig;
struct TileLocator;

class TileBitDatabase {
public:
    // Access functions

    // Convert TileConfigs to and from actual Tile CRAM
    void config_to_tile_cram(const TileConfig &cfg, CRAMView &tile) const;

    TileConfig tile_cram_to_config(const CRAMView &tile) const;

    // All these functions are designed to be thread safe during fuzzing and database modification
    // Maybe we should have faster unsafe versions too, as that will be the majority of the use cases?
    vector<string> get_sinks() const;

    MuxBits get_mux_data_for_sink(const string &sink) const;

    vector<string> get_settings_words() const;

    WordSettingBits get_data_for_setword(const string &name) const;

    vector<string> get_settings_enums() const;

    EnumSettingBits get_data_for_enum(const string &name) const;
    // TODO: function to get routing graph of tile

    // Add relevant items to the database
    void add_mux(const MuxBits &mux);

    void add_setting_word(const WordSettingBits &wsb);

    void add_setting_enum(const EnumSettingBits &esb);

    // Save the bit database to file
    void save();

    // Function to obtain the singleton BitDatabase for a given tile
    friend shared_ptr<TileBitDatabase> get_tile_bitdata(const TileLocator &tile);

private:
    explicit TileBitDatabase(const string &filename);

    mutable boost::shared_mutex db_mutex;
    atomic<bool> dirty{false};
    map<string, MuxBits> muxes;
    map<string, WordSettingBits> words;
    map<string, EnumSettingBits> enums;
    string filename;

    void load();
};

}

#endif //LIBTRELLIS_BITDATABASE_HPP
