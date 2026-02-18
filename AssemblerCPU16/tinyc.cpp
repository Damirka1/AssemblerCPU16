#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>

enum TokenType {
    TOK_INT, TOK_ID, TOK_NUM, TOK_IF, TOK_WHILE, TOK_ELSE, TOK_RETURN,
    TOK_INPUT, TOK_PRINT, // Новые токены для IO
    TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_SEMI, TOK_ASSIGN,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_EQ, TOK_NEQ, TOK_EOF,
    TOK_ASM
};

struct Token {
    TokenType type;
    std::string value;
};

struct Variable {
    int offset;     // Смещение относительно R8 (для локальных) или 0 (для глобальных)
    bool is_local;
    std::string name;
};

class Lexer {
    std::string src;
    size_t pos = 0;
public:
    Lexer(std::string s) : src(s) {}

    Token next() {
        while (pos < src.size() && isspace(src[pos])) pos++;
        if (pos >= src.size()) return { TOK_EOF, "" };

        if (isalpha(src[pos])) {
            std::string id;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) id += src[pos++];
            if (id == "int") return { TOK_INT, id };
            if (id == "if") return { TOK_IF, id };
            if (id == "else") return { TOK_ELSE, id };
            if (id == "while") return { TOK_WHILE, id };
            if (id == "return") return { TOK_RETURN, id };
            if (id == "input") return { TOK_INPUT, id }; // Ввод
            if (id == "print") return { TOK_PRINT, id }; // Вывод
            if (id == "__asm") return { TOK_ASM, id };
            return { TOK_ID, id };
        }

        if (isdigit(src[pos])) {
            std::string num;
            while (pos < src.size() && isdigit(src[pos])) num += src[pos++];
            return { TOK_NUM, num };
        }

        if (src.substr(pos, 2) == "==") { pos += 2; return { TOK_EQ, "==" }; }
        if (src.substr(pos, 2) == "!=") { pos += 2; return { TOK_NEQ, "!=" }; }

        char c = src[pos++];
        switch (c) {
        case '{': return { TOK_LBRACE, "{" };
        case '}': return { TOK_RBRACE, "}" };
        case '(': return { TOK_LPAREN, "(" };
        case ')': return { TOK_RPAREN, ")" };
        case ';': return { TOK_SEMI, ";" };
        case '=': return { TOK_ASSIGN, "=" };
        case '+': return { TOK_PLUS, "+" };
        case '-': return { TOK_MINUS, "-" };
        case '*': return { TOK_STAR, "*" };
        case '/': return { TOK_SLASH, "/" };
        }
        return { TOK_EOF, "" };
    }
};

class Compiler {
    Lexer lexer;
    Token current;
    std::stringstream code_ss;
    std::stringstream data_ss;

    // Таблицы символов
    std::map<std::string, Variable> globals;
    std::map<std::string, Variable> locals;

    int label_counter = 0;
    int local_stack_offset = 0; // Смещение локальных переменных в стеке
    std::string current_func_exit_label; // Метка выхода для return

    void next() { current = lexer.next(); }

    void expect(TokenType t) {
        if (current.type != t) throw std::runtime_error("Syntax Error: Unexpected token " + current.value);
        next();
    }

    std::string new_label() {
        return "L_" + std::to_string(label_counter++);
    }

    void emit(std::string instr) { code_ss << "    " << instr << "\n"; }
    void emit_label(std::string lbl) { code_ss << lbl << ":\n"; }

public:
    Compiler(std::string src) : lexer(src), current({ TOK_EOF, "" }) {
        next();
    }

    // Поиск переменной (сначала локальные, потом глобальные)
    Variable getVar(const std::string& name) {
        if (locals.count(name)) return locals[name];
        if (globals.count(name)) return globals[name];
        throw std::runtime_error("Undefined variable: " + name);
    }

    void parseFactor() {
        if (current.type == TOK_NUM) {
            emit("MOV R0, " + current.value);
            next();
        }
        else if (current.type == TOK_ID) {
            Variable v = getVar(current.value);
            next();
            if (v.is_local) {
                // Локальная: [R8 - offset]
                // R8 - это Base Pointer текущего кадра
                emit("MOV R0, [R8 + -" + std::to_string(v.offset) + "]");
            }
            else {
                // Глобальная: [DATA + Name]
                emit("MOV R0, [DATA + " + v.name + "]");
            }
        }
        else if (current.type == TOK_INPUT) {
            // Чтение из порта 0
            next(); expect(TOK_LPAREN); expect(TOK_RPAREN);
            emit("IN R0, 0");
        }
        else if (current.type == TOK_LPAREN) {
            next();
            parseExpression();
            expect(TOK_RPAREN);
        }
    }

    void parseTerm() {
        parseFactor();
        while (current.type == TOK_STAR || current.type == TOK_SLASH) {
            TokenType op = current.type;
            next();
            emit("PUSH R0");
            parseFactor();
            emit("POP R1");
            if (op == TOK_STAR) emit("MUL R1, R0");
            else emit("DIV R1, R0"); // Предполагаем DIV R1, R0 (R1 / R0 -> R1)
            emit("MOV R0, R1");
        }
    }

    void parseExpression() {
        parseTerm();
        while (current.type == TOK_PLUS || current.type == TOK_MINUS) {
            TokenType op = current.type;
            next();
            emit("PUSH R0");
            parseTerm();
            emit("POP R1");
            if (op == TOK_PLUS) emit("ADD R1, R0");
            else emit("SUB R1, R0");
            emit("MOV R0, R1");
        }
    }

    void parseStatement() {
        if (current.type == TOK_INT) {
            // Локальная переменная: int x;
            next();
            std::string name = current.value;
            expect(TOK_ID);
            expect(TOK_SEMI);

            local_stack_offset += 2; // Резервируем 2 байта (1 слово)
            locals[name] = { local_stack_offset, true, name };
            emit("SUB SP, 2"); // Выделяем место на стеке
        }
        else if (current.type == TOK_ID) {
            // Присваивание
            std::string name = current.value;
            Variable v = getVar(name);
            next();
            expect(TOK_ASSIGN);
            parseExpression();
            expect(TOK_SEMI);

            if (v.is_local) {
                emit("MOV [R8 + -" + std::to_string(v.offset) + "], R0");
            }
            else {
                emit("MOV [DATA + " + v.name + "], R0");
            }
        }
        else if (current.type == TOK_PRINT) {
            // Вывод: print(expr);
            next(); expect(TOK_LPAREN);
            parseExpression();
            expect(TOK_RPAREN); expect(TOK_SEMI);
            emit("OUT 0, R0"); // Пишем в порт 0
        }
        else if (current.type == TOK_IF) {
            next(); expect(TOK_LPAREN); parseExpression(); expect(TOK_RPAREN);
            std::string skip_lbl = new_label();
            emit("CMP R0, 0");
            emit("JZ " + skip_lbl);
            parseStatement();
            emit_label(skip_lbl);
        }
        else if (current.type == TOK_WHILE) {
            next();
            std::string start_lbl = new_label();
            std::string end_lbl = new_label();
            emit_label(start_lbl);
            expect(TOK_LPAREN); parseExpression(); expect(TOK_RPAREN);
            emit("CMP R0, 0");
            emit("JZ " + end_lbl);
            parseStatement();
            emit("JMP " + start_lbl);
            emit_label(end_lbl);
        }
        else if (current.type == TOK_LBRACE) {
            next();
            while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
                parseStatement();
            }
            expect(TOK_RBRACE);
        }
        else if (current.type == TOK_RETURN) {
            next();
            if (current.type != TOK_SEMI) parseExpression();
            expect(TOK_SEMI);
            // Прыгаем в конец функции для очистки стека
            emit("JMP " + current_func_exit_label);
        }
        else if (current.type == TOK_ASM) {
            next(); expect(TOK_LPAREN); next(); expect(TOK_RPAREN); expect(TOK_SEMI);
        }
        else {
            if (current.type == TOK_SEMI) next();
            else throw std::runtime_error("Unknown statement: " + current.value);
        }
    }

    void parseFunction() {
        // Формат: int name() { ... }
        next(); // пропускаем int
        std::string name = current.value;
        expect(TOK_ID);
        expect(TOK_LPAREN); expect(TOK_RPAREN);

        emit_label(name);

        // --- ПРОЛОГ ФУНКЦИИ ---
        // Сохраняем старый R8 (Frame Pointer), устанавливаем новый R8 = SP
        emit("PUSH R8");
        emit("MOV R8, SP");

        // Сброс локальных переменных для новой функции
        locals.clear();
        local_stack_offset = 0;
        current_func_exit_label = name + "_EXIT";

        // Тело
        parseStatement(); // Обрабатывает {...}

        // --- ЭПИЛОГ ФУНКЦИИ ---
        emit_label(current_func_exit_label);
        // Восстанавливаем SP (удаляем локальные переменные)
        emit("MOV SP, R8");
        // Восстанавливаем старый Frame Pointer
        emit("POP R8");
        emit("RET");
    }

    void parseProgram() {
        while (current.type != TOK_EOF) {
            if (current.type == TOK_INT) {
                // Предварительный просмотр, чтобы понять: переменная или функция
                // Ограничение: LL(2) нужно, но лексер простой.
                // Хак: смотрим лексером вперед или предполагаем структуру
                // В данном простом C: глобальные переменные не инициализируются при объявлении int x;

                // Чтобы не усложнять лексер, просто сохраним состояние
                Token temp_type = current;
                std::string name = lexer.next().value; // Имя
                Token next_tok = lexer.next(); // Что дальше?

                // Пересоздаем лексер (грубый откат), или просто парсим с проверкой
                // Проще: перепишем логику чуть-чуть.
                // Но раз у нас токены в потоке, сделаем правильно:
                // Мы "съели" токены. Так нельзя в LL(1) без буфера.
                // Но для TinyC: Если после ID идет (, это функция. Иначе переменная.
            }
            // ПЕРЕЗАПУСК ЛОГИКИ ПАРСИНГА (упрощенно, так как нет `peek`)
            // Мы знаем, что текущий токен INT.
            next(); // съели int
            std::string name = current.value;
            expect(TOK_ID);

            if (current.type == TOK_LPAREN) {
                // Это функция!
                expect(TOK_RPAREN);

                emit_label(name);
                emit("PUSH R8");
                emit("MOV R8, SP");

                locals.clear();
                local_stack_offset = 0;
                current_func_exit_label = name + "_EXIT";

                parseStatement(); // Тело функции

                emit_label(current_func_exit_label);
                emit("MOV SP, R8");
                emit("POP R8");
                emit("RET");
            }
            else if (current.type == TOK_SEMI) {
                // Глобальная переменная
                globals[name] = { 0, false, name };
                data_ss << name << ": .RESW 1\n";
                next();
            }
            else {
                throw std::runtime_error("Expected ; or ( after identifier");
            }
        }
    }

    std::string getAssembly() {
        std::stringstream ss;
        ss << "; Generated by TinyC for CPU16\n";
        ss << ".DATA\n";
        ss << "ZERO: .WORD 0\n";
        ss << data_ss.str();
        ss << "\n.CODE\n";
        ss << ".ORG 0x0000\n";
        ss << "    MOV SP, 0xFFF0 ; Инициализация стека (примерный адрес)\n";
        ss << "    CALL main\n";
        ss << "    HLT\n";
        ss << code_ss.str();
        return ss.str();
    }
};

//int main(int argc, char** argv) {
//    // Обновленный код на C
//    // Читаем количество чисел, затем сами числа, суммируем их и выводим.
//    std::string c_code = R"(
//        int sum;
//        
//        int main() {
//            int count;
//            int val;
//            
//            sum = 0;
//            
//            count = input();
//            
//            while (count) {
//                val = input();
//                sum = sum + val;
//                print(sum);  
//                count = count - 1;
//            }
//            
//            return sum;
//        }
//    )";
//
//    try {
//        Compiler compiler(c_code);
//        compiler.parseProgram();
//        std::string asm_code = compiler.getAssembly();
//
//        std::cout << "--- C CODE ---\n" << c_code << "\n";
//        std::cout << "\n--- GENERATED ASSEMBLY ---\n" << asm_code << "\n";
//
//        std::ofstream out("output.asm");
//        out << asm_code;
//        out.close();
//        std::cout << "Saved to output.asm\n";
//
//    }
//    catch (const std::exception& e) {
//        std::cerr << "Compile Error: " << e.what() << std::endl;
//        return 1;
//    }
//    return 0;
//}