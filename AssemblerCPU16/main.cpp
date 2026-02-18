#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <map>
#include <iomanip>
#include <cctype>
#include <algorithm>

enum Opcode {
    NOP = 0x00,
    MOV = 0x01,  // MOV ds, src / JMP src
    STR = 0x02,  // MOV [ds], src
    JZ = 0x03,  // JZ src
    LDR = 0x04,  // MOV ds, [src]
    CMP = 0x05,  // CMP ds, src
    JNZ = 0x06,  // JNZ src
    POP = 0x07,  // POP ds
    JC = 0x08,  // JC src
    JNC = 0x09,  // JNC src
    JS = 0x0A,  // JS src
    IN = 0x0B,  // IN ds, src
    OUT = 0x0C,  // OUT ds, src
    JNS = 0x0D,  // JNS src
    PUSH = 0x0F, // PUSH src
    HLT = 0x10,
    WAIT = 0x11,
    STR_OFF = 0x12, // MOV [ds + imm], src
    LDR_OFF = 0x14, // MOV ds, [src + imm]
    CALL = 0x15,
    RET = 0x16,
    JO = 0x18, // Jump if Overflow
    JNO = 0x19, // Jump if Not Overflow
    JL = 0x1A, // Jump if Less (Signed)
    JGE = 0x1B, // Jump if Greater Equal (Signed)

    // ALU (Bit 7 = 1)
    ADD = 0x80,
    SUB = 0x81,
    AND = 0x82,
    OR = 0x83,
    XOR = 0x84,
    LSL = 0x85,
    LSR = 0x86,
    INC = 0x87,
    MUL = 0x88,
    DIV = 0x89,
    DEC = 0x8F,
    ADC = 0x90, // Add with Carry
    SBB = 0x91  // Subtract with Borrow
};

// Индексы регистров
std::map<std::string, int> REGISTERS = {
    {"DATA", 0}, {"R0", 1}, {"R1", 2}, {"R2", 3}, {"R3", 4},
    {"R4", 5},   {"R5", 6}, {"R6", 7}, {"R7", 8}, {"R8", 9},
    {"IP", 10},  {"SP", 11}, {"RP", 12}, {"BP", 13}
};

// Убираем пробелы по краям
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Перевод в верхний регистр
std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// Парсинг числа (hex или dec)
int parse_number(const std::string& s) {
    if (s.empty()) return 0;
    try {
        if (s.size() > 2 && to_upper(s.substr(0, 2)) == "0X")
            return std::stoi(s.substr(2), nullptr, 16);
        return std::stoi(s);
    }
    catch (...) { return 0; }
}

// Проверка, является ли строка регистром
bool is_register(const std::string& s) {
    return REGISTERS.find(s) != REGISTERS.end();
}

// Перевод в hex4
std::string hex4(uint16_t value) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(4) << value;
    return ss.str();
}

// Тип строки: Инструкция, Данные или Смена адреса
enum LineType { INSTRUCTION, RAW_DATA, RES_DATA, ORG_DIRECTIVE};
enum SectionType { SEC_CODE, SEC_DATA, SEC_BSS };

struct ParsedLine {
    int line_num;
    std::string raw_text;
    SectionType section;
    LineType type;      // Тип строки
    int address = 0;        // Адрес в словах (относительно начала своей секции в pass1, абсолютный в pass2)

    // Для инструкций
    std::string label = "";
    std::string opcode = "";
    std::string arg1 = "";
    std::string arg2 = "";
    bool is_imm = false;
    int imm_value = 0;
    std::string imm_label = "";

    // Для данных (.WORD, .STRING)
    std::vector<uint16_t> raw_data;
    int res_size = 0; // для .RESW
};

class Assembler {
private:
    std::map<std::string, int> labels; // Имя -> Смещение в секции
    std::map<std::string, SectionType> label_sections;
    std::map<std::string, int> constants;
    std::vector<ParsedLine> program;
    std::map<int, uint16_t> memory_map;

    int section_offsets[3] = { 0, 0, 0 }; // Для автоматического размещения
    int section_bases[3] = { 0, 0, 0 };

public:
    // Проход 1: 
    // Парсинг, метки, директивы
    void pass1(std::istream& in) {
        std::string line_raw;
        SectionType current_sec = SEC_CODE;
        int line_count = 0;

        while (std::getline(in, line_raw)) {
            line_count++;
            std::string line = line_raw.substr(0, line_raw.find(';'));
            line = trim(line);
            if (line.empty()) continue;

            if (to_upper(line).rfind("CONST", 0) == 0) {
                std::stringstream ss(line);
                std::string cmd, name, val_str; ss >> cmd >> name >> val_str;
                constants[to_upper(name)] = parse_number(val_str);
                continue;
            }

            std::string up_line = to_upper(line);
            if (up_line == ".CODE" || up_line == ".TEXT") { current_sec = SEC_CODE; continue; }
            if (up_line == ".DATA") { current_sec = SEC_DATA; continue; }
            if (up_line == ".BSS") { current_sec = SEC_BSS;  continue; }

            // .ORG
            if (up_line.rfind(".ORG", 0) == 0) {
                std::stringstream ss(line);
                std::string cmd, val; ss >> cmd >> val;
                section_offsets[current_sec] = parse_number(val);
                continue;
            }

            // Метки labels
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string label_name = to_upper(trim(line.substr(0, colon)));
                labels[label_name] = section_offsets[current_sec];
                label_sections[label_name] = current_sec;
                line = trim(line.substr(colon + 1));
                if (line.empty()) continue;
            }

            ParsedLine pl;
            pl.line_num = line_count;
            pl.raw_text = line_raw;
            pl.section = current_sec;
            pl.address = section_offsets[current_sec];
            pl.is_imm = false;
            pl.res_size = 0;

            std::replace(line.begin(), line.end(), ',', ' ');
            std::stringstream ss(line);
            ss >> pl.opcode; pl.opcode = to_upper(pl.opcode);

            if (pl.opcode == ".WORD" || pl.opcode == "DW") {
                pl.type = RAW_DATA;
                std::string val_str;
                while (ss >> val_str) {
                    if (constants.count(to_upper(val_str))) pl.raw_data.push_back(constants[to_upper(val_str)]);
                    else pl.raw_data.push_back(parse_number(val_str));
                }
                section_offsets[current_sec] += pl.raw_data.size();
            }
            else if (pl.opcode == ".STRING") {
                pl.type = RAW_DATA;
                std::string content; std::getline(ss, content);
                size_t first_q = content.find('"'), last_q = content.rfind('"');
                if (first_q != std::string::npos && last_q > first_q) {
                    std::string text = content.substr(first_q + 1, last_q - first_q - 1);
                    for (char c : text) pl.raw_data.push_back((uint16_t)c);
                    pl.raw_data.push_back(0);
                }
                section_offsets[current_sec] += pl.raw_data.size();
            }
            else if (pl.opcode == ".RESW") {
                pl.type = RES_DATA;
                std::string val; ss >> val;
                pl.res_size = parse_number(val);
                section_offsets[current_sec] += pl.res_size;
            }
            else {
                pl.type = INSTRUCTION;
                if (ss >> pl.arg1) pl.arg1 = to_upper(pl.arg1);
                if (ss >> pl.arg2) pl.arg2 = to_upper(pl.arg2);

                static const std::vector<std::string> imm_cmds = {
                    "JMP", "JZ", "JNZ", "JC", "JNC", "JS", "JNS", "JO", "JNO", "JL", "JGE", "CALL", "PUSH"
                };

                bool arg1_br = (!pl.arg1.empty() && pl.arg1.front() == '[');
                bool arg2_br = (!pl.arg2.empty() && pl.arg2.front() == '[');
                bool has_off = (pl.arg1.find('+') != std::string::npos || pl.arg2.find('+') != std::string::npos);
                bool is_imm_cmd = std::find(imm_cmds.begin(), imm_cmds.end(), pl.opcode) != imm_cmds.end();
                bool arg2_imm = (!pl.arg2.empty() && !is_register(pl.arg2) && !arg2_br);

                if (has_off || arg2_imm || (is_imm_cmd && !pl.arg1.empty() && !is_register(pl.arg1) && !arg1_br)) {
                    pl.is_imm = true;

                    std::string imm_str = "";
                    if (has_off) {
                        std::string arg = (pl.arg1.find('+') != std::string::npos) ? pl.arg1 : pl.arg2;
                        size_t plus = arg.find('+');
                        imm_str = trim(arg.substr(plus + 1, arg.size() - plus - 2));
                    }
                    else if (is_imm_cmd) {
                        imm_str = pl.arg1;
                    }
                    else {
                        imm_str = pl.arg2;
                    }

                    std::string upper_imm = to_upper(imm_str);

                    if (isdigit(imm_str[0]) || imm_str[0] == '-' || (imm_str.size() > 1 && upper_imm.substr(0, 2) == "0X")) {
                        pl.imm_value = parse_number(imm_str);
                        pl.imm_label = ""; // Это число, имени нет
                    }
                    else {
                        // Это имя (либо константа, либо метка)
                        pl.imm_label = upper_imm;

                        // Если это константа, мы уже знаем её значение
                        if (constants.count(upper_imm)) {
                            pl.imm_value = constants[upper_imm];
                        }
                    }

                    //if (constants.count(to_upper(imm_str))) {
                    //    pl.imm_value = constants[to_upper(imm_str)];
                    //}
                    //else if (isdigit(imm_str[0]) || imm_str[0] == '-' || (imm_str.size() > 1 && imm_str.substr(0, 2) == "0X")) {
                    //    pl.imm_value = parse_number(imm_str);
                    //    pl.imm_label = "";
                    //}
                    //else {
                    //    pl.imm_label = to_upper(imm_str); // Теперь метка сохранится!
                    //    if (constants.count(upper_imm)) {
                    //        pl.imm_value = constants[upper_imm];
                    //    }
                    //}

                    section_offsets[current_sec] += 2;
                }
                else {
                    section_offsets[current_sec] += 1;
                }
            }
            program.push_back(pl);
        }

        // Автоматическое распределение секций: CODE -> DATA -> BSS
        section_bases[SEC_CODE] = 0;
        section_bases[SEC_DATA] = section_offsets[SEC_CODE];
        section_bases[SEC_BSS] = section_bases[SEC_DATA] + section_offsets[SEC_DATA];

        // Финализация адресов меток
        for (auto& [name, addr] : labels) {
            addr += section_bases[label_sections[name]];
        }
    }

    // Проход 2: 
    // Генерация карты памяти
    void pass2() {
        std::cout << "\n--- ASSEMBLY LISTING ---\n";
        std::cout << "ADDR  | CODE      | SOURCE\n";
        std::cout << "----------------------------\n";

        for (auto& pl : program) {
            // Применяем базу секции
            int abs_addr = pl.address + section_bases[pl.section];
            std::stringstream hex_code;
            std::string label_info = "";
            
            if (pl.type == RAW_DATA) {
                for (int i = 0; i < pl.raw_data.size(); ++i) {
                    memory_map[abs_addr + i] = pl.raw_data[i];
                    if (i < 2) hex_code << std::hex << std::setw(4) << std::setfill('0') << pl.raw_data[i] << " ";
                }
            }
            else if (pl.type == INSTRUCTION) {
                int ds = 0, src = 0, op = NOP;
                std::string o = pl.opcode;

                auto parse_mem = [&](std::string arg, int& r, int& off) {
                    if (arg.empty() || arg.front() != '[') return false;
                    std::string c = arg.substr(1, arg.size() - 2);
                    size_t p = c.find('+');
                    if (p != std::string::npos) {
                        r = REGISTERS[trim(c.substr(0, p))];
                        off = parse_number(trim(c.substr(p + 1)));
                        return true;
                    }
                    r = REGISTERS[trim(c)]; off = 0; return false;
                    };

                int rb, ro;
                if (o == "MOV" || o == "LDR" || o == "STR") {
                    if (parse_mem(pl.arg1, rb, ro)) { op = STR_OFF; ds = rb; src = REGISTERS[pl.arg2]; pl.imm_value = ro; }
                    else if (parse_mem(pl.arg2, rb, ro)) { op = LDR_OFF; ds = REGISTERS[pl.arg1]; src = rb; pl.imm_value = ro; }
                    else if (pl.arg1.find('[') != std::string::npos) { op = STR; ds = REGISTERS[pl.arg1.substr(1, pl.arg1.size() - 2)]; src = REGISTERS[pl.arg2]; }
                    else if (pl.arg2.find('[') != std::string::npos) { op = LDR; ds = REGISTERS[pl.arg1]; src = REGISTERS[pl.arg2.substr(1, pl.arg2.size() - 2)]; }
                    else { op = MOV; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                }
                else if (o == "ADD") { op = ADD; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "SUB") { op = SUB; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "ADC") { op = ADC; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "SBB") { op = SBB; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "CMP") { op = CMP; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "AND") { op = AND; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "OR") { op = OR;  ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                else if (o == "XOR") { op = XOR; ds = REGISTERS[pl.arg1]; src = pl.is_imm ? 0 : REGISTERS[pl.arg2]; }
                
                else if (o == "JMP") { op = MOV; ds = REGISTERS["IP"]; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JZ") { op = JZ;  src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JNZ") { op = JNZ; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JC") { op = JC;  src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JNC") { op = JNC; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JS") { op = JS;  src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JNS") { op = JNS; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JO") { op = JO;  src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JNO") { op = JNO; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JL") { op = JL;  src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "JGE") { op = JGE; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                
                else if (o == "CALL") { op = CALL; }
                else if (o == "RET") { op = RET; }
                else if (o == "PUSH") { op = PUSH; src = pl.is_imm ? 0 : REGISTERS[pl.arg1]; }
                else if (o == "POP") { op = POP; ds = REGISTERS[pl.arg1]; }
                else if (o == "INC") { op = INC; ds = REGISTERS[pl.arg1]; }
                else if (o == "DEC") { op = DEC; ds = REGISTERS[pl.arg1]; }
                else if (o == "HLT") { op = HLT; }

                uint16_t word = (src << 12) | (ds << 8) | (op & 0xFF);
                memory_map[abs_addr] = word;
                hex_code << std::hex << std::setw(4) << std::setfill('0') << word << " ";

                if (pl.is_imm) {
                    uint16_t imm = 0;
                    if (!pl.imm_label.empty()) {
                        // Проверяем, не метка ли это (адрес перехода)
                        if (labels.count(pl.imm_label)) {
                            imm = (uint16_t)(labels[pl.imm_label] * 2);
                            label_info = " [@" + hex4(imm) + "]";
                        }
                        // Проверяем, не константа ли это (значение CONST)
                        else if (constants.count(pl.imm_label)) {
                            imm = (uint16_t)constants[pl.imm_label];
                            label_info = " [#" + std::to_string(imm) + "]"; // Префикс # для констант
                        }
                        else {
                            label_info = " [!UNDEF!]";
                        }
                    }
                    else {
                        // Это просто число в коде (например, MOV R0, 5)
                        imm = (uint16_t)pl.imm_value;
                    }
                    memory_map[abs_addr + 1] = imm;
                    hex_code << std::hex << std::setw(4) << std::setfill('0') << imm;
                }
            }

            // Запись в листинг
            std::cout << std::right << std::hex << std::setfill('0') << std::setw(4) << (abs_addr * 2) << " | "
                << std::left << std::setfill(' ') << std::setw(10) << hex_code.str() << " | " << pl.raw_text << label_info << "\n";
        }
    }

    void write_mif(const std::string& filename) {
        std::ofstream out(filename);
        if (!out) {
            std::cerr << "Cannot open output file" << std::endl;
            return;
        }

        auto depth = 819;

        out << "DEPTH = " + std::to_string(depth) + ";\n";
        out << "WIDTH = 16;\n";
        out << "ADDRESS_RADIX = HEX;\n";
        out << "DATA_RADIX = HEX;\n";
        out << "CONTENT\nBEGIN\n";

        for (size_t i = 0; i < memory_map.size(); ++i) {
            // Пишем только если не 0 или если нужно (можно оптимизировать размер файла)
            out << std::dec << i            // индекс в десятичном
                << " : "
                << std::hex << std::uppercase
                << std::setw(4) << std::setfill('0')
                << memory_map[i]          // данные в HEX
                << ";\n";
        }

        if (memory_map.size() < depth) {
            out << "[" << std::dec << memory_map.size()
                << ".." << depth - 1
                << "] : 0000;\n";
        }

        out << "END;\n";
        std::cout << "Successfully generated " << filename << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: assembler input.asm [output.mif]" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = (argc > 2) ? argv[2] : "memory.mif";

    std::ifstream in(input_file);
    if (!in) {
        std::cerr << "Cannot open input file: " << input_file << std::endl;
        return 1;
    }

    Assembler asm_core;
    asm_core.pass1(in);
    asm_core.pass2();
    asm_core.write_mif(output_file);

    return 0;
}