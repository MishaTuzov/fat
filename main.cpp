#include <iostream>
#include <exception>
#include <memory>
#include <set>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

const std::vector<std::string> find_mouth = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string to_lower_case(std::string const&s) {
    auto t = s;
    for (int i = 0; i < s.size(); i++) {
        t[i] = std::tolower(s[i]);
    }
    return t;
}

unsigned int char_to_uint(char c) {
    return static_cast<unsigned int>(static_cast<unsigned char>(c));
}

std::string remove_whitespace(std::string const& s) {
    std::string result;
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}

std::string remove_char_from_str(std::string const&s, char d) {
    std::string result;
    for (char c : s) {
        if (c != d) {
            result += c;
        }
    }
    return result;
}

namespace FAT {
const std::uint32_t MIN_SECTOR_SIZE = 512;
const std::uint32_t BYTE = 256;
const std::uint32_t WORD = BYTE * BYTE;
const std::uint32_t DWORD = WORD * WORD;
const std::uint32_t MULS[] = {1, BYTE, WORD, WORD * BYTE, DWORD, DWORD * BYTE, DWORD * WORD, DWORD * WORD * BYTE};

const std::uint32_t FAT32_FREE = 0x00000000;
const std::uint32_t FAT32_CLASTER_MIN = 0x00000002;
const std::uint32_t FAT32_CLASTER_MAX = 0x0FFFFFEF;
const std::uint32_t FAT32_BAD = 0x0FFFFFF7;
const std::uint32_t FAT32_EOC_MIN = 0x0FFFFFF8;
const std::uint32_t FAT32_EOC_MAX = 0x0FFFFFFF;

const std::uint32_t FAT16_FREE = 0x00000000;
const std::uint32_t FAT16_CLASTER_MIN = 0x0002;
const std::uint32_t FAT16_CLASTER_MAX = 0xFFEF;
const std::uint32_t FAT16_BAD = 0xFFF7;
const std::uint32_t FAT16_EOC_MIN = 0xFFF8;
const std::uint32_t FAT16_EOC_MAX = 0xFFFF;

enum FAT_TYPES {
    NOT_FAT,
    FAT12,
    FAT16,
    FAT32,
};

struct File_info {
    std::string name = "";
    std::string name_no_whitespace = "";
    std::string long_name = "";
    std::string long_name_lower = "";
    std::uint_fast32_t attr = 0;
    std::uint32_t low_claster_index = 0;
    std::uint32_t high_claster_index = 0;
    std::uint32_t claster_index = 0;
    std::uint64_t size = 0;
    std::uint32_t date_modify = 0;
    std::uint32_t year_modify = 0;
    std::uint32_t month_modify = 0;
    std::uint32_t day_modify = 0;
    std::uint32_t time_modify = 0;
    std::uint32_t hour_modify = 0;
    std::uint32_t minute_modify = 0;
    std::uint32_t second_modify = 0;
    std::uint32_t time_modified = 0;
    bool is_folder = false;
    bool is_deleted = false;
};

struct LFN_chain {
    std::string name_part = "";
    std::uint32_t check_sum = 0;
    std::uint32_t attr = 0;
};

struct Folder {
    std::vector<std::string> path;
    std::vector<File_info> files;
    std::uint32_t max_size_len = 0;
    std::uint32_t files_amount = 0;
    std::uint32_t dirs_amount = 0;

    std::uint32_t deleted_files_amount = 0;
    std::uint32_t deleted_dirs_amount = 0;
};

class Disk {
        FILE* fd;
        
        FAT_TYPES fat_type = FAT_TYPES::NOT_FAT;

        std::uint32_t SECTOR_SIZE = 0; 
        std::uint32_t SECTOR_PER_CLASTER = 0; 
        std::uint32_t RESERVED_SECTOR_AMOUNT = 0;
        std::uint32_t FAT_TABLE_AMOUNT = 0;
        std::uint32_t TOTAL_SECTOR_AMOUNT = 0;
        std::uint32_t FAT_TABLE_SECTOR_AMOUNT = 0;
        std::uint32_t ROOT_CATALOG_CLASTER_INDEX = 0;
        std::uint32_t ROOT_ENTRIES = 0;
        std::uint32_t ROOT_ENT_CNT = 0;

        std::uint32_t BYTES_PER_CLASTER = 0;
        std::uint32_t FAT_TABLE_SIZE = 0;
        std::uint32_t FIRST_FAT_SECTOR = 0;
        std::uint32_t FIRST_DATA_SECTOR = 0;
        std::uint32_t FIRST_ROOT_DIR_SECTOR = 0;
        std::uint32_t ROOT_DIR_SECTORS = 0;
        std::uint32_t DATA_SECTORS = 0;
        std::uint32_t COUNT_OF_CLUSTERS = 0;
    
        std::uint32_t FAT_FREE = 0;
        std::uint32_t FAT_CLASTER_MIN = 0;
        std::uint32_t FAT_CLASTER_MAX = 0;
        std::uint32_t FAT_BAD = 0;
        std::uint32_t FAT_EOC_MIN = 0;
        std::uint32_t FAT_EOC_MAX = 0;
        std::uint32_t FAT_INDEX_LEN = 0;

    private:
        std::uint64_t extract_with_endian(std::string const& s, int from, int amount, bool is_little_endian = true) {
            std::uint64_t result = 0;
            if (is_little_endian) {
                for (int i = 0; i < amount; i++) {
                    result += static_cast<unsigned char>(s[from + i]) * MULS[i];
                }
            }
            else {
                for (int i = 0; i < amount; i++) {
                    result += static_cast<unsigned char>(s[from + i]) * MULS[amount - i - 1];
                }
            }
            return result;
        }

        void read_boot_sector() {
            auto sector_info = read();

            fat_type = FAT_TYPES::NOT_FAT;

            SECTOR_SIZE = extract_with_endian(sector_info, 0x0b, 2);
            SECTOR_PER_CLASTER = extract_with_endian(sector_info, 0x0d, 1);
            RESERVED_SECTOR_AMOUNT = extract_with_endian(sector_info, 0x0e, 2);
            FAT_TABLE_AMOUNT = extract_with_endian(sector_info, 0x10, 1);
            ROOT_ENTRIES = extract_with_endian(sector_info, 0x11, 2);

            if (extract_with_endian(sector_info, 0x16, 2) == 0) {
                FAT_TABLE_SECTOR_AMOUNT = extract_with_endian(sector_info, 0x24, 4);
            } else {
                FAT_TABLE_SECTOR_AMOUNT = extract_with_endian(sector_info, 0x16, 2);
            }
            if (extract_with_endian(sector_info, 0x13, 2) == 0) {
                TOTAL_SECTOR_AMOUNT = extract_with_endian(sector_info, 0x20, 4);
            } else {
                TOTAL_SECTOR_AMOUNT = extract_with_endian(sector_info, 0x13, 2);
            }
            
            ROOT_CATALOG_CLASTER_INDEX = extract_with_endian(sector_info, 0x2c, 4);
            ROOT_ENT_CNT = extract_with_endian(sector_info, 0x11, 2);
            BYTES_PER_CLASTER = SECTOR_SIZE * SECTOR_PER_CLASTER;
            FAT_TABLE_SIZE = FAT_TABLE_SECTOR_AMOUNT * SECTOR_SIZE;
            FIRST_FAT_SECTOR = RESERVED_SECTOR_AMOUNT;
            ROOT_DIR_SECTORS = ((ROOT_ENT_CNT * 32) + (SECTOR_SIZE - 1)) / SECTOR_SIZE;
            FIRST_ROOT_DIR_SECTOR = FIRST_FAT_SECTOR + FAT_TABLE_AMOUNT * FAT_TABLE_SECTOR_AMOUNT;
            FIRST_DATA_SECTOR = FIRST_ROOT_DIR_SECTOR + ROOT_DIR_SECTORS;
            DATA_SECTORS = TOTAL_SECTOR_AMOUNT - FIRST_DATA_SECTOR;
            COUNT_OF_CLUSTERS = DATA_SECTORS / SECTOR_PER_CLASTER;

            if(COUNT_OF_CLUSTERS < 4085) {
                std::cout << "FAT12" << std::endl;
                std::cout << "WARNING! NOT SUPPORTED!" << std::endl;
                fat_type = FAT_TYPES::FAT12;
            } else if(COUNT_OF_CLUSTERS < 65525) {
                std::cout << "FAT16" << std::endl;
                fat_type = FAT_TYPES::FAT16;

                FAT_FREE = FAT16_FREE;
                FAT_CLASTER_MIN = FAT16_CLASTER_MIN;
                FAT_CLASTER_MAX = FAT16_CLASTER_MAX;
                FAT_BAD = FAT16_BAD;
                FAT_EOC_MIN = FAT16_EOC_MIN;
                FAT_EOC_MAX = FAT16_EOC_MAX;
                FAT_INDEX_LEN = 2;
            } else {
                std::cout << "FAT32" << std::endl;
                fat_type = FAT_TYPES::FAT32;

                FAT_FREE = FAT32_FREE;
                FAT_CLASTER_MIN = FAT32_CLASTER_MIN;
                FAT_CLASTER_MAX = FAT32_CLASTER_MAX;
                FAT_BAD = FAT32_BAD;
                FAT_EOC_MIN = FAT32_EOC_MIN;
                FAT_EOC_MAX = FAT32_EOC_MAX;
                FAT_INDEX_LEN = 4;
            }

            std::cout << "Sector size " << SECTOR_SIZE << std::endl;
            std::cout << "Sector per claster " << SECTOR_PER_CLASTER << std::endl;
            std::cout << "RESERVED_SECTOR_AMOUNT " << RESERVED_SECTOR_AMOUNT << std::endl;
            std::cout << "FAT_TABLE_AMOUNT " << FAT_TABLE_AMOUNT << std::endl;
            std::cout << "TOTAL_SECTOR_AMOUNT " << TOTAL_SECTOR_AMOUNT << std::endl;
            std::cout << "FAT_TABLE_SECTOR_AMOUNT " << FAT_TABLE_SECTOR_AMOUNT << std::endl;
            std::cout << "ROOT_CATALOG_CLASTER_INDEX " << ROOT_CATALOG_CLASTER_INDEX << std::endl;
            std::cout << "BYTES_PER_CLASTER " << BYTES_PER_CLASTER << std::endl;
            std::cout << "FAT_TABLE_SIZE " << FAT_TABLE_SIZE << std::endl;
            std::cout << "FIRST_FAT_SECTOR " << FIRST_FAT_SECTOR << std::endl;
            std::cout << "FIRST_DATA_SECTOR " << FIRST_DATA_SECTOR << std::endl;
            std::cout << "COUNT_OF_CLUSTERS " << COUNT_OF_CLUSTERS << std::endl;
            std::cout << "FIRST_ROOT_DIR_SECTOR " << FIRST_ROOT_DIR_SECTOR << std::endl;
        }
    
        File_info parse_file(std::string const& s, int offset) {
            File_info file_info;
            file_info.name = s.substr(offset, 11);
            file_info.name_no_whitespace = remove_whitespace(file_info.name);
            file_info.low_claster_index = extract_with_endian(s, offset + 0x1a, 2);
            file_info.high_claster_index = extract_with_endian(s, offset + 0x14, 2);
            file_info.size = extract_with_endian(s, offset + 0x1c, 4);
            file_info.attr = extract_with_endian(s, offset + 0x0b, 1);
            
            file_info.date_modify = extract_with_endian(s, offset + 0x18, 2);
            file_info.day_modify = file_info.date_modify & 0x1F;
            file_info.month_modify = ((file_info.date_modify & 0x1e0) >> 5) - 1;
            file_info.year_modify = ((file_info.date_modify & 0xfe00) >> 9) + 1980;

            file_info.time_modify = extract_with_endian(s, offset + 0x16, 2);
            file_info.second_modify = (file_info.date_modify & 0x1F) * 2;
            file_info.minute_modify = ((file_info.date_modify & 0x7e0) >> 5);
            file_info.hour_modify = ((file_info.date_modify & 0xf800) >> 11);

            file_info.claster_index = file_info.low_claster_index + file_info.high_claster_index * WORD;
            if (file_info.name[0] == static_cast<char>(0xe5)) {
                file_info.is_deleted = true;
            }
            file_info.is_folder = static_cast<bool>(file_info.attr & 0x10);
            return file_info;
        }

        LFN_chain parse_LFN(std::string const& s, int offset) {
            LFN_chain lfn;
            for (int i = 0; i < 10; i += 2) {
                char c = s[offset + 0x01 + i];
                if (c == char(0x00) || c == char(0xFF)) break;
                lfn.name_part.push_back(c);
            }
            for (int i = 0; i < 12; i += 2) {
                char c = s[offset + 0x0E + i];
                if (c == char(0x00) || c == char(0xFF)) break;
                lfn.name_part.push_back(c);
            }            
            for (int i = 0; i < 4; i += 2) {
                char c = s[offset + 0x1C + i];
                if (c == char(0x00) || c == char(0xFF)) break;
                lfn.name_part.push_back(c);
            }
            lfn.check_sum = extract_with_endian(s, offset + 0x0d, 1);
            lfn.attr = extract_with_endian(s, offset + 0x0b, 1);
            return lfn;
        }

        std::uint32_t get_next_claster_index(std::uint32_t current) {
            if (current > COUNT_OF_CLUSTERS + 1 || current > FAT_CLASTER_MAX || current < FAT_CLASTER_MIN) {
                throw std::string("Current claster index is out of range");
            }

            std::uint32_t byte_addres = FIRST_FAT_SECTOR * SECTOR_SIZE + (current) * FAT_INDEX_LEN;
            std::string sector = read_sector(byte_addres / SECTOR_SIZE);
            return extract_with_endian(sector, byte_addres % SECTOR_SIZE, FAT_INDEX_LEN);
        }

        std::vector<std::uint32_t> get_claster_chain(std::uint32_t start) {
            std::uint32_t current = start;
            std::vector<std::uint32_t> chain;
            while (FAT_CLASTER_MIN <= current && current <= std::min(FAT_CLASTER_MAX, COUNT_OF_CLUSTERS + 1)) {
                chain.push_back(current);
                current = get_next_claster_index(current);
            }
            if (current == FAT_BAD) {
                throw std::string("Chain of clusters end in bad cluster");
            }
            if (!(FAT_EOC_MIN <= current && current <= FAT_EOC_MAX)) {
                std::cerr << "Bad Cluster index : 0x" << std::hex << current << std::dec << std::endl;
                throw std::string("Chain of clusters end in not EOC cluster");
            }
            return chain;
        }
    public:
        Disk() {
            fd = nullptr;
        }

        bool is_mounted() {
            return fd != nullptr;
        }

        void mount(std::string path) {
            fd = fopen(path.c_str(), "rb");
            if (!fd) {
                throw std::string("Failed to mount disk : ") + path;
            }
            read_boot_sector();
        }

        void unmount() {
            if (!fd) return;

            if (fclose(fd) == EOF) {
                throw std::string("Failed to unmount disk");
            }
            fd = nullptr;
        }

        FILE* get() {
            return fd;
        }

        void seek(int position) {
            if (fseek(fd, position, SEEK_SET)) {
                throw std::string("Error in fseek") + std::to_string(position);
            }
        }

        std::string continue_reading() {
            char data[MIN_SECTOR_SIZE];

            fread(data, MIN_SECTOR_SIZE, 1, fd);
            if (ferror(fd)) {
                throw std::string("Error in file reading");
            }

            return std::string(data, MIN_SECTOR_SIZE);
        }

        std::string read(int position = 0) {
            seek(position);
            return continue_reading();
        }

        std::string read_sector(int sector_pos) {
            seek(sector_pos * SECTOR_SIZE);
            std::string answer;
            for (int i = 0; i < SECTOR_SIZE / MIN_SECTOR_SIZE; i++) {
                answer += continue_reading();
            }
            return answer;
        }

        std::string read_cluster(int cluster_pos) {
            seek(((cluster_pos - 2) * SECTOR_PER_CLASTER + FIRST_DATA_SECTOR) * SECTOR_SIZE);
            std::string answer;
            for (int i = 0; i < (SECTOR_SIZE * SECTOR_PER_CLASTER) / MIN_SECTOR_SIZE; i++) {
                answer += continue_reading();
            }
            return answer;
        }

        Folder parse_folder(std::uint32_t first_cluster, std::vector<std::string> previous_path, std::string next="") {
            if (first_cluster == 0) {
                first_cluster = ROOT_CATALOG_CLASTER_INDEX;
            }


            Folder folder;
            std::string LFN = "";

            auto chain = get_claster_chain(first_cluster);

            for (std::uint32_t cluster_index = 0; cluster_index < chain.size(); cluster_index++) {
                auto data = read_cluster(chain[cluster_index]);
                for (int i = 0; i < data.size(); i+=32) {
                    if (extract_with_endian(data, i, 1) == 0x0) {
                        continue; // все же тут break или continue???
                    }
                    File_info file = parse_file(data, i);
                    if (file.attr == 0x0f) {
                        LFN_chain lfn = parse_LFN(data, i);
                        LFN = lfn.name_part + LFN;
                        continue;
                    }
                    file.long_name += LFN;
                    file.long_name_lower = to_lower_case(file.long_name);
                    if (LFN != "") {
                    }
                    LFN = "";
                    folder.files.push_back(file);
                }
            }
            if (next != "")
                previous_path.push_back(next);
            folder.path = previous_path;
            config_folder(folder);
            return folder;
        }

        Folder parse_root_folder() {
            if (fat_type == FAT_TYPES::FAT32) {
                return parse_folder(0, {});
            }
            Folder folder;
            std::string LFN = "";
            for (int pos = FIRST_ROOT_DIR_SECTOR; pos < ROOT_DIR_SECTORS + FIRST_ROOT_DIR_SECTOR; pos++) {
                auto data = read_sector(pos);
                for (int i = 0; i < data.size(); i+=32) {
                    if (extract_with_endian(data, i, 1) == 0x0) {
                        continue; // все же тут break или continue???
                    }
                    File_info file = parse_file(data, i);
                    if (file.attr == 0x0f) {
                        LFN_chain lfn = parse_LFN(data, i);
                        LFN = lfn.name_part + LFN;
                        continue;
                    }
                    file.long_name += LFN;
                    file.long_name_lower = to_lower_case(file.long_name);
                    LFN = "";
                    folder.files.push_back(file);
                }
            }

            config_folder(folder);
            return folder;
        }

        void get_file(std::uint32_t first_claster, std::string &destination) {
            auto chain = get_claster_chain(first_claster);
            destination = "";
            for (int i = 0; i < chain.size(); i++) {
                destination += read_cluster(chain[i]);
            }
        }

        static std::string get_file_show_name(File_info const& file) {
            if (file.long_name != "") {
                return file.long_name;
            }
            std::string name = file.name.substr(0, 8), ext = file.name.substr(8);
            if (file.is_deleted) {
                name[0] = '?';
            }
            name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
            ext.erase(std::remove(ext.begin(), ext.end(), ' '), ext.end());
            if (ext != "") {
                ext = "." + ext;
            }
            return name + ext;
        }

        static bool cmp_files_name(File_info const&a, File_info const&b) {
            return to_lower_case(get_file_show_name(a)) < to_lower_case(get_file_show_name(b));
        }

        std::int64_t count_size(FAT::Folder const& folder) {
            std::int64_t sz = 0;
            for (auto file : folder.files) {
                if (file.is_folder) {
                    if (file.name_no_whitespace == "." || file.name_no_whitespace == "..") continue;
                    auto sub_folder = parse_folder(file.claster_index, {});
                    sz += count_size(sub_folder);
                } else {
                    sz += file.size;
                }
            }
            return sz;
        }

        void config_folder(Folder & folder) {
            std::sort(folder.files.begin(), folder.files.end(), cmp_files_name);
            std::uint64_t max_size = 0;
            for (auto file : folder.files) {
                max_size = std::max(max_size, file.size);
                if (file.is_folder) {
                    if (!file.is_deleted)
                        folder.dirs_amount ++;
                    else
                        folder.deleted_dirs_amount++;
                } else {
                    if (!file.is_deleted)
                        folder.files_amount ++;
                    else
                        folder.deleted_files_amount++;
                }
            }
            folder.max_size_len = 1;
            std::uint64_t mul = 1;
            while (mul < max_size) {
                mul *= 10;
                folder.max_size_len ++;
            }
        }

        ~Disk() {
            try {
                unmount();
            } catch (std::string err) {
                std::cerr << err << std::endl;
            } catch (...) {
                std::cerr << "Some unknown error in Disk destructor occured" << std::endl;
            }
        }
    };
}


class Terminal {
    FAT::Folder current_folder, root_folder, temp_folder;
    FAT::Disk disk;
public:
    void mount(std::string path) {
        disk.mount(path);
        root_folder = disk.parse_root_folder();
        current_folder = root_folder;
    }

    void unmount() {
        disk.unmount();
        current_folder = FAT::Folder();
    }

    bool is_mounted() {
        return disk.is_mounted();
    }

    void pwd() {
        std::cout << "/";
        for (auto el : current_folder.path) {
            std::cout << el << "/";
        }
        std::cout << std::endl;
    }

    bool find_name(FAT::Folder const& folder, std::string const& find_name, bool is_long_name, FAT::File_info &file_req, bool show_deleted) {
        for (auto file : folder.files) {
            if (!show_deleted && file.is_deleted) continue;
            if (file.long_name_lower == find_name && is_long_name || file.name_no_whitespace == find_name && !is_long_name) {
                file_req = file;
                return true;
            }
        }
        file_req = FAT::File_info();
        return false;
    }

    bool find_file_in_folder(FAT::Folder const& folder, std::string const&name, FAT::File_info& file, bool show_deleted) {
        if (name == ".") {
            file = FAT::File_info();
            file.name = ".";
            return true;
        }
        if(find_name(folder, to_lower_case(name), true, file, show_deleted))
            return true;
        
        std::string temp_name = remove_whitespace(name);
        if (temp_name != ".." && temp_name != ".")
            temp_name = remove_char_from_str(temp_name, '.');

        if (char_to_uint(temp_name[0]) == '?') {
            temp_name[0] = char(0xe5);
        }
        if(find_name(folder, temp_name, false, file, show_deleted))
            return true;
        
        return false;
    }

    bool cd_one(std::string const& name, FAT::Folder const& folder, FAT::Folder &folder_req, bool show_deleted) {
        if (name == "~") {
            folder_req = root_folder;
            return true;
        }
        if (name == ".") {
            folder_req = folder;
            return true;
        }
        FAT::File_info file;
        if(!find_file_in_folder(folder, name, file, show_deleted)) {
            folder_req = FAT::Folder();
            return false;
        }
        
        if (file.claster_index == 0) {
            folder_req = root_folder;
            return true;
        }
        folder_req = disk.parse_folder(file.claster_index, folder.path, name);
        if(folder_req.path.size() > 0 && folder_req.path.back() == "..") {
            folder_req.path.pop_back();
            if (folder_req.path.size() > 0) {
                folder_req.path.pop_back();
            }
        }
        return true;
    }

    bool go_to_folder(std::string path, FAT::Folder const& folder, FAT::Folder &destination_folder, bool show_deleted) {
        std::string atom;
        std::stringstream ss(path);
        temp_folder = folder;
        while (std::getline(ss, atom, '/')) {
            if(!cd_one(atom, temp_folder, temp_folder, show_deleted)) {
                std::cout << "No such folder : " << path << std::endl;
                destination_folder = FAT::Folder();
                return false;
            }
        }
        destination_folder = temp_folder;
        return true;
    } 

    void cd(std::string path, std::string const& config) {
        bool show_deleted = (config.find("d") != std::string::npos);
        if(go_to_folder(path, current_folder, temp_folder, show_deleted)) {
            current_folder = temp_folder;
        }
    }

    void open_file(std::string const& path, std::string &destination, bool show_deleted) {
        FAT::Folder folder;
        std::string file_name;

        if (path.find('/') != std::string::npos) {
            if (!go_to_folder(path.substr(0, path.find_last_of('/')), current_folder, folder, show_deleted))
                return;
            file_name = path.substr(path.find_last_of('/') + 1);
        } else {
            folder = current_folder;
            file_name = path;
        }
        FAT::File_info file;
        if (!find_file_in_folder(folder, file_name, file, show_deleted)) {
            std::cout << "There is no such file : " << path << std::endl;
            return;
        }
        disk.get_file(file.claster_index, destination);
        destination.erase(destination.begin() + file.size, destination.end());
    }

    void cat(std::string const& path, std::string const& config) {
        bool show_deleted = (config.find("d") != std::string::npos);
        std::string file_data;
        open_file(path, file_data, show_deleted);
        std::cout << file_data << std::endl << std::endl;
    }

    void ls(std::string const&config) {
        pwd();
        bool show_deleted = (config.find("d") != std::string::npos);
        bool show_hidden = (config.find("h") != std::string::npos);

        if (config.find("l") == std::string::npos) {
            for (auto file : current_folder.files) {
                if (!show_deleted && file.is_deleted) continue;
                if (!show_hidden && (file.name_no_whitespace == "." || file.name_no_whitespace == "..")) continue;
                std::cout << disk.get_file_show_name(file);
                if (file.is_folder) std::cout << "/";
                std::cout << "\t\t";
            }
            std::cout << std::endl;
            return;
        }
        for (auto file : current_folder.files) {
            if (!show_deleted && file.is_deleted) continue;
            if (!show_hidden && (file.name_no_whitespace == "." || file.name_no_whitespace == "..")) continue;

            printf("-rw-r--r-- 1\t");
            printf("%*llu\t", current_folder.max_size_len, static_cast<unsigned long long>(file.size));
            printf("%s %02u %04u\t", find_mouth[file.month_modify].c_str(), file.day_modify, file.year_modify);
            std::cout << disk.get_file_show_name(file);
            if (file.is_folder) std::cout << "/";
            if (file.is_deleted) std::cout << " [deleted]";
            std::cout << std::endl;
        }
    }

    void dir(std::string const&config) {
        bool show_deleted = (config.find("d") != std::string::npos);
        bool show_short = (config.find("x") != std::string::npos);
        
        std::cout << "Dirrectory of ";
        pwd();
        std::cout << std::endl;

        for (auto file : current_folder.files) {
            if (!show_deleted && file.is_deleted) continue;

            if (file.is_deleted)
                std::cout << "[DELETED]\t"; 
            
            printf("%02u/%02u/%04u\t", file.month_modify + 1, file.day_modify, file.year_modify);
            printf("%02u:%02u\t", file.hour_modify, file.minute_modify);
                
            if (file.is_folder) std::cout << "<DIR>";
            else std::cout << "     ";
            printf("  %*llu\t", current_folder.max_size_len, static_cast<unsigned long long>(file.size));

            
            if (!show_short)
                std::cout << disk.get_file_show_name(file);
            else
                std::cout << file.name;
            std::cout << std::endl;
        }

        auto files_amount = current_folder.files_amount;
        auto dirs_amount = current_folder.dirs_amount;
        if (show_deleted) {
            files_amount += current_folder.deleted_files_amount;
            dirs_amount += current_folder.deleted_dirs_amount;
        }

        std::cout << "\t\t\t\t" << files_amount << " File(s) " << disk.count_size(current_folder) << " bytes" << std::endl;
        std::cout << "\t\t\t\t" << dirs_amount << " Dir(s)" << std::endl;
    }

    std::int64_t get_size(std::string const& path) {
        FAT::Folder folder;
        if (!go_to_folder(path, current_folder, folder, false))
            return -1;
        return disk.count_size(folder);
    }

    void copy(std::string const& source, std::string const& destination, std::string const& config) {
        bool show_deleted = (config.find("d") != std::string::npos);
        std::string file_data;
        open_file(source, file_data, show_deleted);
        auto fd = fopen(destination.c_str(), "wb+");
        if (!fd) {
            throw std::string("Failed to open file : " + destination);
        }

        fwrite(file_data.c_str(), file_data.size(), 1, fd);
        if (fclose(fd) == EOF) {
            throw std::string("Failed to close file : " + destination);
        }
    }
};


int main() {
    try {
        bool should_work = true;
        Terminal terminal;
        std::string command;
        std::set <std::string> commands_inside_disk = {"unmount",
            "pwd", "ls", "dir", "cd", "size", "cat", "cp", "copy"};

        while(should_work) {
            std::string temp, config, prev;
            std::vector<std::string> paths;
            std::string full_command;
            std::getline(std::cin, full_command);
            std::stringstream line(full_command);
            std::getline(line, command, ' ');
            while (std::getline(line, temp, ' ')) {
                temp = prev + temp;
                if (temp[0] == '-' || temp[0] == '/') {
                    config += temp;
                    continue;
                }
                if (temp.back() == '\\') {
                    prev = temp.substr(0, temp.size() - 1) + " ";
                    continue;
                }
                paths.push_back(temp);
                prev = "";
            }
            if (command == "help") {
                std::cout << "This FAT manager is created by Misha Tuzov AI360" << std::endl;
            } else if (command == "exit" || command == "2") {
                should_work = false;
                break;
            } else if (command == "1" || command == "mount") {
                terminal.unmount();
                
                terminal.mount(paths[0]);
                std::cout << "Disk " << paths[0] << " mounted" << std::endl;
            } else if (true) {
                if (!terminal.is_mounted()) {
                    std::cout << "Disk is not mounted. Mount before using this command" << std::endl; 
                } else if (command == "unmount" || command == "3") {
                    terminal.unmount();
                    std::cout << "Disk unmounted" << std::endl;
                } else if (command == "pwd") {
                    terminal.pwd();
                } else if (command == "ls") {
                    terminal.ls(config);
                } else if (command == "dir") {
                    terminal.dir(config);
                } else if (command == "cd") {
                    terminal.cd(paths[0], config);
                } else if (command == "size") {
                    std::int64_t result = terminal.get_size(paths[0]);
                    if (result != -1)
                        std::cout << "Size of folder " << paths[0] << " : " << result << " bytes" << std::endl;
                } else if (command == "cat") {
                    terminal.cat(paths[0], config);
                } else if (command == "copy" || command == "cp") {
                    terminal.copy(paths[0], paths[1], config);
                }
            } else {
                std::cout << "No such command : " << command << std::endl;
            }
        }
    } catch(std::exception e) {
        std::cerr << e.what() << std::endl;
    } catch(std::string e) {
        std::cerr << e << std::endl;
    } catch(...) {
        std::cerr << "Some unknown error occured" << std::endl;
    }
}