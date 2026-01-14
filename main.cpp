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

std::string to_lower_case(std::string &s) {
    auto t = s;
    for (int i = 0; i < s.size(); i++) {
        t[i] = std::tolower(s[i]);
    }
    return t;
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
    std::string long_name = "";
    std::string long_name_lower = "";
    std::uint_fast32_t attr = 0;
    std::uint32_t low_claster_index = 0;
    std::uint32_t high_claster_index = 0;
    std::uint32_t claster_index = 0;
    std::uint32_t size = 0;
    std::uint32_t date_last_access = 0;
    std::uint32_t year_last_access = 0;
    std::uint32_t month_last_access = 0;
    std::uint32_t day_last_access = 0;
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
};

class Disk {
        FILE* fd;
        
        //Consts
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
        std::uint64_t extract_with_endian(std::string& s, int from, int amount, bool is_little_endian = true) {
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
        }
    
        File_info parse_file(std::string& s, int offset) {
            File_info file_info;
            file_info.name = s.substr(offset, 11);
            file_info.low_claster_index = extract_with_endian(s, offset + 0x1a, 2);
            file_info.high_claster_index = extract_with_endian(s, offset + 0x14, 2);
            file_info.size = extract_with_endian(s, offset + 0x1c, 4);
            file_info.attr = extract_with_endian(s, offset + 0x0b, 1);
            
            // file_info.date_last_access = extract_with_endian(s, offset + 0x12, 2);
            // file_info.day_last_access = file_info.date_last_access & 0x1F;
            // file_info.month_last_access = (file_info.date_last_access & 0x1e0) >> 5;
            // file_info.year_last_access = (file_info.date_last_access & 0xfe00) >> 9;

            file_info.claster_index = file_info.low_claster_index + file_info.high_claster_index * WORD;
            if (file_info.name[0] == static_cast<char>(0xe5) || file_info.name[0] == static_cast<char>(0x05)) {
                file_info.is_deleted = true;
            }
            return file_info;
        }

        LFN_chain parse_LFN(std::string& s, int offset) {
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
            if (current > COUNT_OF_CLUSTERS + 1 && current < FAT_CLASTER_MIN || current > FAT_CLASTER_MAX) {
                throw std::string("Current claster index is out of range");
            }
            // std::cout << "We seek to " << FIRST_FAT_SECTOR * SECTOR_SIZE + (current - 2) * FAT_INDEX_LEN << std::endl;
            // seek(FIRST_FAT_SECTOR * SECTOR_SIZE + (current - 2) * FAT_INDEX_LEN);

            // char buffer[4];
            // fread(buffer, FAT_INDEX_LEN, 1, fd);
            // if (ferror(fd)) {
            //     throw std::string("Error in file reading");
            // }
            // std::string temp(buffer, FAT_INDEX_LEN);
            // return extract_with_endian(temp, 0, FAT_INDEX_LEN);

            std::uint32_t byte_addres = FIRST_FAT_SECTOR * SECTOR_SIZE + (current) * FAT_INDEX_LEN;
            std::string sector = read_sector(byte_addres / SECTOR_SIZE);
            return extract_with_endian(sector, byte_addres % SECTOR_SIZE, FAT_INDEX_LEN);
        }
    
        void dump_fat_for_cluster(std::uint32_t cluster) {
            std::uint32_t byte_addres = FIRST_FAT_SECTOR * SECTOR_SIZE + (cluster) * FAT_INDEX_LEN;
            std::cout << "Here is FAT table around cluster : " << std::dec << cluster << " with byte address " << std::hex << byte_addres << std::dec << std::endl;
            std::string s = read_sector(byte_addres / SECTOR_SIZE);
            for (int i = 0; i < s.size(); i++) {
                std::cout << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(s[i])) << " ";
            }
            std::cout << std::dec << std::endl;
            std::uint32_t i = byte_addres % SECTOR_SIZE;
            std::cout << "Here is exact value of asked dword : " << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(s[i])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+1])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+2])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+3])) << std::dec << std::endl;


            byte_addres = FIRST_FAT_SECTOR * SECTOR_SIZE + FAT_TABLE_SECTOR_AMOUNT * SECTOR_SIZE + (cluster) * FAT_INDEX_LEN;
            std::cout << "Here is FAT2 table around cluster : " << std::dec << cluster << " with byte address " << std::hex << byte_addres << std::dec << std::endl;
            s = read_sector(byte_addres / SECTOR_SIZE);
            for (int i = 0; i < s.size(); i++) {
                std::cout << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(s[i])) << " ";
            }
            std::cout << std::dec << std::endl;
            i = byte_addres % SECTOR_SIZE;
            std::cout << "Here is exact value of asked dword : " << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(s[i])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+1])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+2])) << " "
            << static_cast<unsigned int>(static_cast<unsigned char>(s[i+3])) << std::dec << std::endl;
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
            std::cout << "Current position is  " << ((cluster_pos - 2) * SECTOR_PER_CLASTER + FIRST_DATA_SECTOR) * SECTOR_SIZE << std::endl;
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
            std::uint32_t cluster_index = first_cluster;
            Folder folder;
            std::string LFN = "";
            dump_fat_for_cluster(cluster_index);

                std::cout << "=== DEBUG parse_folder ===" << std::endl;
    std::cout << "first_cluster = " << first_cluster << " (hex: 0x" << std::hex << first_cluster << std::dec << ")" << std::endl;
    std::cout << "fat_type = " << fat_type << std::endl;
    std::cout << "COUNT_OF_CLUSTERS = " << COUNT_OF_CLUSTERS << std::endl;
    std::cout << "FAT_CLASTER_MIN = " << FAT_CLASTER_MIN << std::endl;
    std::cout << "FAT_CLASTER_MAX = " << FAT_CLASTER_MAX << std::endl;
    std::cout << "FAT_EOC_MIN = 0x" << std::hex << FAT_EOC_MIN << std::dec << std::endl;
    std::cout << "FAT_EOC_MAX = 0x" << std::hex << FAT_EOC_MAX << std::dec << std::endl;
    int cluster_count = 0;

            while (FAT_CLASTER_MIN <= cluster_index && cluster_index <= std::min(FAT_CLASTER_MAX, COUNT_OF_CLUSTERS + 1)) {
                cluster_count++;
        std::cout << "\n--- Reading cluster #" << cluster_count << " ---" << std::endl;
        std::cout << "cluster_index = " << cluster_index << " (hex: 0x" << std::hex << cluster_index << std::dec << ")" << std::endl;


                std::cout << "Current cluster index is " << cluster_index << std::endl;
                auto data = read_cluster(cluster_index);
                        std::cout << "Data size = " << data.size() << " bytes" << std::endl;


        // Проверяем, не пустые ли данные
        bool all_zero = true;
        for (char c : data) {
            if (c != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            std::cout << "WARNING: Cluster contains only zeros!" << std::endl;
        }
        int valid_entries = 0;
        for (int i = 0; i < data.size(); i+=32) {
            if (extract_with_endian(data, i, 1) == 0x0) {
                continue;
            }
            valid_entries++;
        }
        std::cout << "Valid directory entries in cluster: " << valid_entries << std::endl;

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

                std::cout << "Getting next cluster..." << std::endl;
                std::cout << "Before cluster index is " << cluster_index << std::endl;
                cluster_index = get_next_claster_index(cluster_index);
                std::cout << "After cluster index is " << cluster_index << std::endl;


                // Сразу проверяем EOC
        if (cluster_index >= FAT_EOC_MIN && cluster_index <= FAT_EOC_MAX) {
            std::cout << "EOC detected! Stopping." << std::endl;
            break;
        }
        
        // Защита от бесконечного цикла
        if (cluster_count > 100) {
            std::cout << "ERROR: Too many clusters! Possible infinite loop." << std::endl;
            break;
        }
            }
            if (cluster_index == FAT_BAD) {
                throw std::string("Chain of clusters end in bad cluster");
            }
            if (!(FAT_EOC_MIN <= cluster_index && cluster_index <= FAT_EOC_MAX)) {
                std::cerr << "Bad Cluster index : 0x" << std::hex << cluster_index << std::dec << std::endl;
                throw std::string("Chain of clusters end in not EOC cluster");
            }
            if (next != "")
                previous_path.push_back(next);
            folder.path = previous_path;
            std::cout << "folder is parsed" << std::endl;
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

            std::cout << "folder is parsed" << std::endl;
            return folder;
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
    void Init(FAT::Disk& disk) {
        if (!disk.is_mounted())
            throw std::string("Disk is not mounted. Impossible to Init Terminal");
        root_folder = disk.parse_root_folder();
        current_folder = root_folder;
    }

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

    void show_folder(FAT::Folder folder) {
        for (int i = 0; i < folder.files.size() && i < 10; i++) {
            std::cout << "0x" << std::hex << folder.files[i].attr << "\tis del : " << folder.files[i].is_deleted << "\tfirst byte : "
             << static_cast<unsigned int>(static_cast<unsigned char>(folder.files[i].name[0])) << std::dec
             << "\tLFN size : " << folder.files[i].long_name.size() << "\t";
            if (folder.files[i].long_name != "") {
                std::cout << folder.files[i].long_name; // << std::endl;
                // for (int j = 0; j < folder.files[i].long_name.size(); j++) {
                //     std::cout << static_cast<unsigned int>(static_cast<unsigned char>(folder.files[i].long_name[j])) << " ";
                // }
            }
            else {
                std::string name = folder.files[i].name.substr(0, 8), ext = folder.files[i].name.substr(8);
                if (folder.files[i].is_deleted) {
                    name[0] = '?';
                }
                name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
                ext.erase(std::remove(ext.begin(), ext.end(), ' '), ext.end());
                if (ext != "") {
                    ext = "." + ext;
                }
                std::cout << name + ext;
            }
            std::cout << std::endl;
        }
    }

    bool check_names(FAT::Folder &folder, std::string find_name, std::string path_name, bool is_long_name) {
        for (auto file : folder.files) {
            if (file.long_name == find_name && is_long_name || file.name == find_name && !is_long_name) {
                if (file.claster_index == 0) {
                    folder = root_folder;
                    return true;
                }
                if (is_long_name)  {
                    path_name = file.long_name;
                }
                folder = disk.parse_folder(file.claster_index, folder.path, path_name);
                return true;
            }
        }
        return false;
    }

    bool cd_one(std::string& name) {
        if (name == ".") return true;        
        if(check_names(temp_folder, to_lower_case(name), "", true))
            return true;
        
        std::string temp_name = name;
        while (temp_folder.files.size() > 0 && temp_name.size() < temp_folder.files[0].name.size()) {
            temp_name.push_back(' ');
        }
        if(check_names(temp_folder, temp_name, name, false))
            return true;
        return false;
    }

    void cd(std::string path) {
        std::string atom;
        std::stringstream ss(path);
        temp_folder = current_folder;
        while (std::getline(ss, atom, '/')) {
            if(!cd_one(atom)) {
                std::cout << "No such folder : " << path << std::endl;
                return;
            }
            if(temp_folder.path.size() > 0 && temp_folder.path.back() == "..") {
                temp_folder.path.pop_back();
                if (temp_folder.path.size() > 0) {
                    temp_folder.path.pop_back();
                }
            }
        }
        current_folder = temp_folder;
        temp_folder = FAT::Folder();
    }

    void ls() {
        pwd();
        show_folder(current_folder);
    }
};


int main() {
    try {
        bool should_work = true;
        // FAT::Disk current_disk;
        Terminal terminal;
        std::string command;
        std::set <std::string> commands_inside_disk = {"unmount",
            "pwd", "ls", "dir", "cd", "size", "cat", "cp", "copy"};

        while(should_work) {
            std::string path, source, destination, file, host_file;
            std::cin>>command;
            if (command == "help") {
                std::cout << "This FAT manager is created by Misha Tuzov AI360" << std::endl;
            } else if (command == "exit" || command == "2") {
                should_work = false;
                break;
            } else if (command == "1" || command == "mount") {
                terminal.unmount();
                
                std::cin >> host_file;
                terminal.mount(host_file);
                std::cout << "Disk " << host_file << " mounted" << std::endl;

                // auto result = current_disk.read();
                // std::cout << std::hex;
                // for (int i = 0; i < result.size(); i++) {
                //     std::cout << static_cast<unsigned int>(static_cast<unsigned char>(result[i])) << " ";
                // }
                // std::cout << std::dec << std::endl;

                // std::cout << std::hex << result << std::dec << std::endl;
                // std::cout << "Was read : " << result.size() << " bytes" << std::endl;
            } else if (true) {
                if (!terminal.is_mounted()) {
                    std::cout << "Disk is not mounted. Mount before using this command" << std::endl; 
                } else if (command == "unmount" || command == "3") {
                    terminal.unmount();
                    std::cout << "Disk unmounted" << std::endl;
                } else if (command == "pwd") {
                    terminal.pwd();
                } else if (command == "ls") {
                    terminal.ls();
                } else if (command == "dir") {
                    std::cout << "Not finished" << std::endl;
                } else if (command == "cd") {
                    std::cin >> path;
                    terminal.cd(path);
                } else if (command == "size") {
                    std::cin >> path;
                    std::cout << "Not finished" << std::endl;
                } else if (command == "cat") {
                    std::cin >> file;
                    std::cout << "Not finished" << std::endl;
                } else if (command == "copy" || command == "cp") {
                    std::cin >> source >> destination;
                    std::cout << "Not finished" << std::endl;
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