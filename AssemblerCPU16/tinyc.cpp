#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>

// Токены
enum TokenType {
    TOK_INT, TOK_CHAR, TOK_VOID, TOK_ID, TOK_NUM, TOK_STR, TOK_IF, TOK_WHILE, TOK_FOR, TOK_ELSE, TOK_RETURN,
    TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_LBRACK, TOK_RBRACK, TOK_SEMI, TOK_ASSIGN,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_AMP, TOK_BANG, TOK_COMMA,
    TOK_INC, TOK_DEC, TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR, TOK_EOF, TOK_ASM
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    Token(TokenType t = TOK_EOF, std::string v = "", int l = 0) : type(t), value(v), line(l) {}
};

// Структура переменной
struct Variable {
    int offset;     // Смещение относительно BP
    bool is_local;
    bool is_array;
    int array_size;
    std::string name;
};

class Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1;
public:
    Lexer(std::string s) : src(s) {}
    Token next() {
        while (pos < src.size()) {
            if (isspace(src[pos])) { if (src[pos] == '\n') line++; pos++; continue; }
            if (src[pos] == '/' && src[pos + 1] == '/') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }
            break;
        }
        if (pos >= src.size()) return { TOK_EOF, "", line };

        if (isalpha(src[pos])) {
            std::string id;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) id += src[pos++];
            if (id == "int") return { TOK_INT, id, line };
            if (id == "char") return { TOK_CHAR, id, line };
            if (id == "void") return { TOK_VOID, id, line };
            if (id == "if") return { TOK_IF, id, line };
            if (id == "else") return { TOK_ELSE, id, line };
            if (id == "while") return { TOK_WHILE, id, line };
            if (id == "for") return { TOK_FOR, id, line };
            if (id == "return") return { TOK_RETURN, id, line };
            if (id == "__asm") return { TOK_ASM, id, line };
            return { TOK_ID, id, line };
        }
        if (isdigit(src[pos])) {
            std::string num;
            while (pos < src.size() && isdigit(src[pos])) num += src[pos++];
            return { TOK_NUM, num, line };
        }
        if (src[pos] == '"') {
            std::string s; pos++;
            while (pos < src.size() && src[pos] != '"') s += src[pos++];
            pos++; return { TOK_STR, s, line };
        }

        if (src.substr(pos, 2) == "==") { pos += 2; return { TOK_EQ, "==", line }; }
        if (src.substr(pos, 2) == "!=") { pos += 2; return { TOK_NEQ, "!=", line }; }
        if (src.substr(pos, 2) == "<=") { pos += 2; return { TOK_LE, "<=", line }; }
        if (src.substr(pos, 2) == ">=") { pos += 2; return { TOK_GE, ">=", line }; } // В логах была опечатка
        if (src.substr(pos, 2) == "++") { pos += 2; return { TOK_INC, "++", line }; }
        if (src.substr(pos, 2) == "--") { pos += 2; return { TOK_DEC, "--", line }; }
        if (src.substr(pos, 2) == "&&") { pos += 2; return { TOK_AND, "&&", line }; }
        if (src.substr(pos, 2) == "||") { pos += 2; return { TOK_OR, "||", line }; }

        char c = src[pos++];
        switch (c) {
        case '{': return { TOK_LBRACE, "{", line }; case '}': return { TOK_RBRACE, "}", line };
        case '(': return { TOK_LPAREN, "(", line }; case ')': return { TOK_RPAREN, ")", line };
        case '[': return { TOK_LBRACK, "[", line }; case ']': return { TOK_RBRACK, "]", line };
        case ';': return { TOK_SEMI, ";", line };   case '=': return { TOK_ASSIGN, "=", line };
        case '+': return { TOK_PLUS, "+", line };   case '-': return { TOK_MINUS, "-", line };
        case '*': return { TOK_STAR, "*", line };   case '/': return { TOK_SLASH, "/", line };
        case '<': return { TOK_LT, "<", line };     case '>': return { TOK_GT, ">", line };
        case '&': return { TOK_AMP, "&", line };    case '!': return { TOK_BANG, "!", line };
        case ',': return { TOK_COMMA, ",", line };
        }
        return { TOK_EOF, "", line };
    }
};


class Compiler {
    Lexer lexer;
    Token current;
    std::stringstream code_ss, data_ss;
    std::map<std::string, Variable> globals, locals;
    int str_counter = 0, label_counter = 0, local_stack_offset = 0;
    std::string current_func_exit_label;

    void next() { current = lexer.next(); }
    bool isType(TokenType t) { return t == TOK_INT || t == TOK_CHAR || t == TOK_VOID; }
    void expect(TokenType t) {
        if (current.type != t)
            throw std::runtime_error("Line " + std::to_string(current.line) + ": Expected token " + std::to_string(t) + " but got '" + current.value + "'");
        next();
    }
    std::string new_label() { return "L" + std::to_string(label_counter++); }
    void emit(std::string instr) { code_ss << "    " << instr << "\n"; }
    void emit_label(std::string lbl) { code_ss << lbl << ":\n"; }

public:
    Compiler(std::string src) : lexer(src) { next(); }

    Variable getVar(const std::string& name) {
        if (locals.count(name)) return locals[name];
        if (globals.count(name)) return globals[name];
        throw std::runtime_error("Line " + std::to_string(current.line) + ": Undefined variable " + name);
    }


    void parseFactor() {
        if (current.type == TOK_NUM) {
            emit("MOV R0, " + current.value); next();
        }
        else if (current.type == TOK_STR) {
            std::string lbl = "STR_" + std::to_string(str_counter++);
            data_ss << lbl << ": .STRING \"" << current.value << "\"\n";
            emit("MOV R0, " + lbl); next();
        }
        else if (current.type == TOK_ID) {
            std::string name = current.value; next();
            if (current.type == TOK_LPAREN) { // Вызов функции
                next(); int args = 0;
                if (current.type != TOK_RPAREN) {
                    while (true) { parseExpression(); emit("PUSH R0"); args++; if (current.type == TOK_COMMA) next(); else break; }
                }
                expect(TOK_RPAREN); emit("CALL " + name);
                if (args > 0) emit("ADD SP, " + std::to_string(args * 2));
            }
            else if (current.type == TOK_LBRACK) { // Массив
                next(); parseExpression(); expect(TOK_RBRACK);
                Variable v = getVar(name);
                emit("PUSH R0");
                if (v.is_local) { emit("MOV R1, BP"); emit("ADD R1, " + std::to_string(-v.offset)); }
                else { emit("MOV R1, " + v.name); }
                emit("POP R0"); emit("ADD R1, R0"); emit("ADD R1, R0"); // *2 (word size)
                emit("MOV R0, [R1]");
            }
            else if (current.type == TOK_INC || current.type == TOK_DEC) {
                bool inc = (current.type == TOK_INC); next();
                Variable v = getVar(name);
                std::string adr = v.is_local ? "[BP + " + std::to_string(-v.offset) + "]" : "[" + v.name + "]";
                emit("MOV R0, " + adr); emit(inc ? "INC R0" : "DEC R0"); emit("MOV " + adr + ", R0");
            }
            else {
                Variable v = getVar(name);
                if (v.is_local) emit("MOV R0, [BP + " + std::to_string(-v.offset) + "]");
                else emit("MOV R0, [" + v.name + "]");
            }
        }
        else if (current.type == TOK_LPAREN) {
            next(); parseExpression(); expect(TOK_RPAREN);
        }
    }

    void parseTerm() {
        parseFactor();
        while (current.type == TOK_STAR || current.type == TOK_SLASH) {
            TokenType op = current.type; next();
            emit("PUSH R0"); parseFactor(); emit("MOV R1, R0"); emit("POP R0");
            emit(op == TOK_STAR ? "MUL R0, R1" : "DIV R0, R1");
        }
    }

    void parseArithm() {
        parseTerm();
        while (current.type == TOK_PLUS || current.type == TOK_MINUS) {
            TokenType op = current.type; next();
            emit("PUSH R0"); parseTerm(); emit("MOV R1, R0"); emit("POP R0");
            emit(op == TOK_PLUS ? "ADD R0, R1" : "SUB R0, R1");
        }
    }

    void parseExpression() {
        if (current.type == TOK_ID) {
            Lexer look = lexer; Token n = look.next();
            if (n.type == TOK_ASSIGN) {
                std::string name = current.value; Variable v = getVar(name);
                next(); next(); parseExpression();
                if (v.is_local) emit("MOV [BP + " + std::to_string(-v.offset) + "], R0");
                else emit("MOV [" + v.name + "], R0");
                return;
            }
        }
        parseArithm();
        if (current.type == TOK_AND || current.type == TOK_OR) {
            TokenType op = current.type; next();
            std::string skip = new_label();
            emit("CMP R0, 0");
            emit(op == TOK_AND ? "JZ " + skip : "JNZ " + skip);
            parseArithm(); emit_label(skip);
        }
    }

    void parseCondition(std::string jump_if_false) {
        parseExpression();
        if (current.type >= TOK_EQ && current.type <= TOK_GE) {
            TokenType op = current.type; next();
            emit("PUSH R0"); parseExpression(); emit("MOV R1, R0"); emit("POP R0");
            emit("CMP R0, R1");
            if (op == TOK_EQ)  emit("JNZ " + jump_if_false);
            else if (op == TOK_NEQ) emit("JZ " + jump_if_false);
            else if (op == TOK_LT)  emit("JGE " + jump_if_false);
            else if (op == TOK_GT)  emit("JLE " + jump_if_false);
            else if (op == TOK_LE)  emit("JG " + jump_if_false);
            else if (op == TOK_GE)  emit("JL " + jump_if_false);
        }
        else { emit("CMP R0, 0"); emit("JZ " + jump_if_false); }
    }

    void parseStatement() {
        if (isType(current.type)) {
            next(); std::string name = current.value; expect(TOK_ID);
            if (current.type == TOK_LBRACK) { // Массив
                next(); int sz = std::stoi(current.value); next(); expect(TOK_RBRACK);
                local_stack_offset += sz * 2;
                locals[name] = { local_stack_offset, true, true, sz, name };
                emit("SUB SP, " + std::to_string(sz * 2) + " ; Alloc array " + name);
                expect(TOK_SEMI);
            }
            else {
                local_stack_offset += 2;
                locals[name] = { local_stack_offset, true, false, 0, name };
                emit("SUB SP, 2 ; Alloc local " + name);
                if (current.type == TOK_ASSIGN) { next(); parseExpression(); emit("MOV [BP + " + std::to_string(-local_stack_offset) + "], R0"); }
                expect(TOK_SEMI);
            }
        }
        else if (current.type == TOK_ID || current.type == TOK_INC || current.type == TOK_DEC) {
            parseExpression(); expect(TOK_SEMI);
        }
        else if (current.type == TOK_IF) {
            next(); expect(TOK_LPAREN); std::string f_lbl = new_label(), e_lbl = new_label();
            parseCondition(f_lbl); expect(TOK_RPAREN); parseStatement();
            if (current.type == TOK_ELSE) { emit("JMP " + e_lbl); emit_label(f_lbl); next(); parseStatement(); emit_label(e_lbl); }
            else emit_label(f_lbl);
        }
        else if (current.type == TOK_WHILE) {
            next(); std::string s_lbl = new_label(), e_lbl = new_label();
            emit_label(s_lbl); expect(TOK_LPAREN); parseCondition(e_lbl); expect(TOK_RPAREN);
            parseStatement(); emit("JMP " + s_lbl); emit_label(e_lbl);
        }
        else if (current.type == TOK_FOR) {
            next(); expect(TOK_LPAREN);
            if (isType(current.type) || current.type == TOK_ID) parseStatement(); else next();
            std::string s_lbl = new_label(), c_lbl = new_label(), e_lbl = new_label(), b_lbl = new_label();
            emit_label(s_lbl);
            if (current.type != TOK_SEMI) parseCondition(e_lbl); expect(TOK_SEMI);
            emit("JMP " + b_lbl);
            emit_label(c_lbl); // Шаг
            if (current.type != TOK_RPAREN) parseExpression();
            expect(TOK_RPAREN); emit("JMP " + s_lbl);
            emit_label(b_lbl); parseStatement(); emit("JMP " + c_lbl); emit_label(e_lbl);
        }
        else if (current.type == TOK_RETURN) {
            next(); if (current.type != TOK_SEMI) parseExpression();
            expect(TOK_SEMI); emit("JMP " + current_func_exit_label);
        }
        else if (current.type == TOK_LBRACE) {
            next(); while (current.type != TOK_RBRACE && current.type != TOK_EOF) parseStatement();
            expect(TOK_RBRACE);
        }
        else if (current.type == TOK_ASM) {
            next(); expect(TOK_LPAREN); std::string s = current.value;
            s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
            emit(s); next(); expect(TOK_RPAREN); expect(TOK_SEMI);
        }
        else if (current.type == TOK_SEMI) next();
    }

    void parseProgram() {
        while (current.type != TOK_EOF) {
            if (!isType(current.type)) throw std::runtime_error("Line " + std::to_string(current.line) + ": Expected type");
            next(); std::string name = current.value; expect(TOK_ID);
            if (current.type == TOK_LPAREN) {
                next(); locals.clear(); local_stack_offset = 0;
                int p_off = 4;
                if (current.type != TOK_RPAREN) {
                    while (true) { isType(current.type); next(); locals[current.value] = { p_off, true, false, 0, current.value }; p_off += 2; expect(TOK_ID); if (current.type == TOK_COMMA) next(); else break; }
                }
                expect(TOK_RPAREN); emit_label(name);
                emit("PUSH BP"); emit("MOV BP, SP");
                current_func_exit_label = name + "_EXIT";
                parseStatement();
                emit_label(current_func_exit_label);
                emit("MOV SP, BP"); emit("POP BP"); emit("RET");
            }
            else if (current.type == TOK_LBRACK) {
                next(); int sz = std::stoi(current.value); next(); expect(TOK_RBRACK);
                if (current.type == TOK_ASSIGN) throw std::runtime_error("Line " + std::to_string(current.line) + ": Global array init not supported");
                globals[name] = { 0, false, true, sz, name };
                data_ss << name << ": .RESW " << sz << "\n";
                expect(TOK_SEMI);
            }
            else if (current.type == TOK_ASSIGN) {
                next(); std::string val = current.value; expect(TOK_NUM);
                globals[name] = { 0, false, false, 0, name };
                data_ss << name << ": .WORD " << val << "\n";
                expect(TOK_SEMI);
            }
            else {
                globals[name] = { 0, false, false, 0, name };
                data_ss << name << ": .RESW 1\n";
                expect(TOK_SEMI);
            }
        }
    }

    std::string getAssembly() {
        std::stringstream ss;
        ss << "; TinyC v1.2 for CPU16\n.DATA\n" << data_ss.str();
        ss << "\n.CODE\n.ORG 0x0000\n    MOV SP, 0xFFFE\n    MOV BP, 0xFFFE\n    CALL main\n    HLT\n";
        ss << code_ss.str(); return ss.str();
    }
};

//int main(int argc, char** argv) {
//    if (argc < 2) {
//        std::cerr << "Usage: " << argv[0] << " <source.c>" << std::endl;
//        return 1;
//    }
//
//    std::ifstream file("program.f");
//    if (!file.is_open()) {
//        std::cerr << "Could not open file: " << argv[1] << std::endl;
//        return 1;
//    }
//
//    std::string c_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
//
//    try {
//        Compiler compiler(c_code);
//        compiler.parseProgram();
//        std::string asm_code = compiler.getAssembly();
//
//        std::ofstream out("output.asm");
//        out << asm_code;
//        out.close();
//        std::cout << "Successfully compiled to output.asm" << std::endl;
//    }
//    catch (const std::exception& e) {
//        std::cerr << "Compile Error: " << e.what() << std::endl;
//        return 1;
//    }
//    return 0;
//}