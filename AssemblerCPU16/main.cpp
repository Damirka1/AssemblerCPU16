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
    MOV = 0x01,  // MOV ds, src
    STR = 0x02,  // MOV [ds], src
    JZ = 0x03,   // JZ src
    LDR = 0x04,  // MOV ds, [src]
    CMP = 0x05,  // CMP ds, src
    POP = 0x07,  // POP ds
    IN = 0x0B,   // IN ds, src
    OUT = 0x0C,  // OUT ds, src
    PUSH = 0x0F, // PUSH src
    HLT = 0x10,
    WAIT = 0x11,

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
    DEC = 0x8F
};

// Индексы регистров
std::map<std::string, int> REGISTERS = {
    {"DATA", 0}, {"R0", 1}, {"R1", 2}, {"R2", 3}, {"R3", 4},
    {"R4", 5},   {"R5", 6}, {"R6", 7}, {"R7", 8}, {"R8", 9},
    {"IP", 10},  {"SP", 11}, {"RP", 12}
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
        if (s.size() > 2 && s.substr(0, 2) == "0X")
            return std::stoi(s.substr(2), nullptr, 16);
        if (s.size() > 2 && s.substr(0, 2) == "0x")
            return std::stoi(s.substr(2), nullptr, 16);
        return std::stoi(s);
    }
    catch (...) {
        return 0;
    }
}

// Проверка, является ли строка регистром
bool is_register(const std::string& s) {
    return REGISTERS.find(s) != REGISTERS.end();
}

// Тип строки: Инструкция, Данные или Смена адреса
enum LineType { INSTRUCTION, RAW_DATA, ORG_DIRECTIVE };

struct ParsedLine {
    int line_num;
    LineType type;      // Тип строки
    int address;        // Адрес в памяти (в словах)

    // Для инструкций
    std::string label;
    std::string opcode;
    std::string arg1;
    std::string arg2;
    bool is_imm;
    int imm_value;
    std::string imm_label;

    // Для данных (.WORD, .STRING)
    std::vector<uint16_t> raw_data;
};

class Assembler {
private:
    std::map<std::string, int> labels;
    std::map<std::string, int> constants; // CONST NAME VALUE
    std::vector<ParsedLine> program;
    std::vector<uint16_t> machine_code;
    int current_addr = 0;

public:
    // Проход 1: 
    // Парсинг, метки, директивы
    void pass1(std::istream& in) {
        std::string line_raw;
        current_addr = 0;
        int line_count = 0;

        while (std::getline(in, line_raw)) {
            line_count++;

            // Убираем комментарии
            std::string line = line_raw.substr(0, line_raw.find(';'));
            size_t comment_slash = line.find("//");
            if (comment_slash != std::string::npos) {
                line = line.substr(0, comment_slash);
            }
            line = trim(line);
            if (line.empty()) continue;

            // Обработка CONST (CONST NAME VALUE)
            // Должно быть в начале строки
            if (line.rfind("CONST", 0) == 0) {
                std::stringstream ss(line);
                std::string cmd, name, val_str;
                ss >> cmd >> name >> val_str;
                constants[to_upper(name)] = parse_number(val_str);
                continue; // Это не код, в память не пишем
            }

            // Ищем метку "label:"
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string label_name = trim(line.substr(0, colon));
                labels[to_upper(label_name)] = current_addr;
                line = trim(line.substr(colon + 1));
                if (line.empty()) continue;
            }

            ParsedLine pl;
            pl.line_num = line_count;
            pl.address = current_addr;
            pl.type = INSTRUCTION; // По умолчанию
            pl.is_imm = false;

            // Разбиваем на токены
            // Сначала заменяем запятые на пробелы
            std::replace(line.begin(), line.end(), ',', ' ');
            std::stringstream ss(line);

            std::string token;
            ss >> token;
            pl.opcode = to_upper(token);

            // ОБРАБОТКА ДИРЕКТИВ
            // .ORG 0x100 - смена адреса
            if (pl.opcode == ".ORG") {
                pl.type = ORG_DIRECTIVE;
                std::string addr_str;
                ss >> addr_str;
                int new_addr = parse_number(addr_str);
                current_addr = new_addr;
                pl.address = new_addr; // Запоминаем новый адрес
                program.push_back(pl);
                continue;
            }

            // .WORD 1234, CONST_VAL, 0x55
            else if (pl.opcode == ".WORD" || pl.opcode == "DW") {
                pl.type = RAW_DATA;
                std::string val_str;
                while (ss >> val_str) {
                    std::string up_val = to_upper(val_str);
                    if (constants.count(up_val)) {
                        pl.raw_data.push_back(constants[up_val]);
                    }
                    else {
                        pl.raw_data.push_back(parse_number(val_str));
                    }
                    current_addr++; // 1 слово = 1 адрес
                }
                program.push_back(pl);
                continue;
            }

            // .STRING "Hello World"
            else if (pl.opcode == ".STRING") {
                pl.type = RAW_DATA;
                // Остаток строки (после .STRING) это контент
                std::string content;
                std::getline(ss, content);

                size_t first_q = content.find('"');
                size_t last_q = content.rfind('"');

                if (first_q != std::string::npos && last_q != std::string::npos && last_q > first_q) {
                    std::string text = content.substr(first_q + 1, last_q - first_q - 1);
                    for (char c : text) {
                        pl.raw_data.push_back((uint16_t)c);
                        current_addr++;
                    }
                    pl.raw_data.push_back(0); // Null terminator
                    current_addr++;
                }
                program.push_back(pl);
                continue;
            }

            // ОБРАБОТКА ИНСТРУКЦИЙ
            std::string temp;
            if (ss >> temp) pl.arg1 = to_upper(temp);
            if (ss >> temp) pl.arg2 = to_upper(temp);

            bool arg1_is_bracket = (!pl.arg1.empty() && pl.arg1.front() == '[');
            bool arg2_is_bracket = (!pl.arg2.empty() && pl.arg2.front() == '[');

            // Анализ типа адресации
            bool needs_imm = false;
            std::string imm_str = "";

            // Случай 1: Второй аргумент - число/метка/константа
            if (!pl.arg2.empty() && !is_register(pl.arg2) && !arg2_is_bracket) {
                needs_imm = true;
                imm_str = pl.arg2;
            }
            // Случай 2: Первый аргумент (для JMP/PUSH) - число/метка/константа
            else if ((pl.opcode == "JMP" || pl.opcode == "JZ" || pl.opcode == "CALL" || pl.opcode == "PUSH")
                && !pl.arg1.empty() && !is_register(pl.arg1) && !arg1_is_bracket) {
                needs_imm = true;
                imm_str = pl.arg1;
            }

            if (needs_imm) {
                pl.is_imm = true;

                // Проверяем, может это CONST?
                if (constants.count(to_upper(imm_str))) {
                    pl.imm_value = constants[to_upper(imm_str)];
                }
                // Проверка: это число?
                else if (isdigit(imm_str[0]) || imm_str[0] == '-' || (imm_str.size() > 1 && imm_str.substr(0, 2) == "0X")) {
                    pl.imm_value = parse_number(imm_str);
                }
                else {
                    pl.imm_label = imm_str; // Значит метка
                }
                current_addr += 2;
            }
            else {
                current_addr += 1;
            }

            program.push_back(pl);
        }
    }

    // Проход 2: 
    // Генерация карты памяти
    void pass2() {
        // Используем map, чтобы поддерживать .ORG (дырки в памяти)
        std::map<int, uint16_t> memory_map;

        for (const auto& cmd : program) {

            // Если это смена адреса, ничего писать не надо, 
            // следующий элемент сам запишется по нужному адресу
            if (cmd.type == ORG_DIRECTIVE) {
                continue;
            }

            // Если это сырые данные (.WORD, .STRING)
            if (cmd.type == RAW_DATA) {
                int ptr = cmd.address;
                for (uint16_t val : cmd.raw_data) {
                    memory_map[ptr++] = val;
                }
                continue;
            }

            // Если это инструкция
            uint16_t word = 0;
            int op_code = 0;
            int ds = 0;
            int src = 0;

            std::string op = cmd.opcode;

            // Логика выбора Opcode и операндов
            if (op == "NOP") op_code = NOP;
            else if (op == "HLT") op_code = HLT;
            else if (op == "WAIT") op_code = WAIT;

            else if (op == "MOV") {
                if (cmd.arg1.front() == '[') { // MOV [R1], R2
                    op_code = STR;
                    ds = REGISTERS[cmd.arg1.substr(1, cmd.arg1.size() - 2)];
                    src = REGISTERS[cmd.arg2];
                }
                else if (cmd.arg2.front() == '[') { // MOV R1, [R2]
                    op_code = LDR;
                    ds = REGISTERS[cmd.arg1];
                    src = REGISTERS[cmd.arg2.substr(1, cmd.arg2.size() - 2)];
                }
                else { // MOV R1, R2 или MOV R1, 50
                    op_code = MOV;
                    ds = REGISTERS[cmd.arg1];
                    src = cmd.is_imm ? 0 : REGISTERS[cmd.arg2];
                }
            }
            else if (op == "JMP") {
                op_code = MOV;
                ds = REGISTERS["IP"];
                src = 0; // Immediate
            }
            else if (op == "JZ") {
                op_code = JZ;
                ds = 0;
                src = cmd.is_imm ? 0 : REGISTERS[cmd.arg1];
            }
            else if (op == "PUSH") {
                op_code = PUSH;
                src = cmd.is_imm ? 0 : REGISTERS[cmd.arg1];
            }
            else if (op == "POP") {
                op_code = POP;
                src = REGISTERS[cmd.arg1];
            }
            else if (op == "IN") { op_code = IN; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "OUT") { op_code = OUT; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "CMP") { op_code = CMP; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }

            // ALU
            else if (op == "ADD") { op_code = ADD; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "SUB") { op_code = SUB; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "AND") { op_code = AND; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "OR") { op_code = OR;  ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "XOR") { op_code = XOR; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "LSL") { op_code = LSL; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "LSR") { op_code = LSR; ds = REGISTERS[cmd.arg1]; src = REGISTERS[cmd.arg2]; }
            else if (op == "INC") { op_code = INC; ds = REGISTERS[cmd.arg1]; src = 0; }
            else if (op == "DEC") { op_code = DEC; ds = REGISTERS[cmd.arg1]; src = 0; }
            else {
                std::cerr << "Line " << cmd.line_num << ": Unknown opcode " << op << std::endl;
                exit(1);
            }

            // Запись инструкции в карту памяти
            word = (uint16_t)((src << 12) | (ds << 8) | (op_code & 0xFF));
            memory_map[cmd.address] = word;

            // Если есть константа (Immediate)
            if (cmd.is_imm) {
                uint16_t imm_val = 0;
                if (!cmd.imm_label.empty()) {
                    if (labels.find(cmd.imm_label) != labels.end()) {
                        imm_val = labels[cmd.imm_label] * 2; // Адресация
                    }
                    else {
                        std::cerr << "Line " << cmd.line_num << ": Undefined label " << cmd.imm_label << std::endl;
                        exit(1);
                    }
                }
                else {
                    imm_val = (uint16_t)cmd.imm_value;
                }
                memory_map[cmd.address + 1] = imm_val;
            }
        }

        // Конвертация карты в линейный массив (vector)
        if (memory_map.empty()) return;

        int max_addr = memory_map.rbegin()->first;
        machine_code.resize(max_addr + 1, 0); // Заполняем нулями

        for (auto const& [addr, val] : memory_map) {
            machine_code[addr] = val;
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

        for (size_t i = 0; i < machine_code.size(); ++i) {
            // Пишем только если не 0 или если нужно (можно оптимизировать размер файла)
            out << std::dec << i            // индекс в десятичном
                << " : "
                << std::hex << std::uppercase
                << std::setw(4) << std::setfill('0')
                << machine_code[i]          // данные в HEX
                << ";\n";
        }

        if (machine_code.size() < depth) {
            out << "[" << std::dec << machine_code.size()
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