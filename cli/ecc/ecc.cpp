#include "../../src/lexer/lexer.h"
#include "../../src/parser/parser.h"
#include "../../src/ast/printer.h"
#include "../../src/codegen/codegen.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

static void print_help(const char *exec)
{
    std::cout << "Usage:\n"
                 "  "
              << exec << " [options] <file.ec | dir>\n"
                         "  "
              << exec << " [options] <file1.ec file2.ec ...>\n"
                         "\n"
                         "Modes:\n"
                         "  ll                Emit LLVM IR only\n"
                         "  debug             Show tokens, AST, and LLVM IR\n"
                         "  help              Show this help\n"
                         "\n"
                         "Options:\n"
                         "  -o <dir>          Output directory (default: current directory)\n"
                         "\n"
                         "Examples:\n"
                         "  "
              << exec << " main.ec\n"
                         "  "
              << exec << " src/ -o build\n"
                         "  "
              << exec << " ll main.ec\n"
                         "  "
              << exec << " debug src/\n";
}

static std::vector<fs::path> collect_sources(const std::vector<std::string> &inputs)
{
    std::vector<fs::path> result;
    for (auto &arg : inputs)
    {
        fs::path p(arg);
        if (fs::is_directory(p))
        {
            for (auto &it : fs::recursive_directory_iterator(p))
            {
                if (it.is_regular_file() && it.path().extension() == ".ec")
                    result.push_back(it.path());
            }
        }
        else if (fs::is_regular_file(p))
        {
            if (p.extension() == ".ec")
                result.push_back(p);
        }
        else
        {
            std::cerr << "No such file/dir: " << p << "\n";
        }
    }
    return result;
}

static std::unique_ptr<ast::Program> compile_frontend(
    const std::vector<fs::path> &sources,
    bool debug_lex = false,
    bool debug_ast = false)
{
    std::unique_ptr<ast::Program> merged = std::make_unique<ast::Program>();
    std::vector<std::unique_ptr<ast::Decl>> struct_decls;
    std::vector<std::unique_ptr<ast::Decl>> other_decls;

    for (const auto &p : sources)
    {
        std::ifstream in(p);
        if (!in)
        {
            std::cerr << "Failed to open: " << p << "\n";
            return nullptr;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        std::string source = buf.str();

        auto lex_err = [&p](int line, int col, const std::string &msg)
        {
            std::cerr << "[lexer error] " << p << ":" << line << ":" << col << " " << msg << "\n";
        };
        auto parse_err = [&p](int line, int col, const std::string &msg)
        {
            std::cerr << "[parser error] " << p << ":" << line << ":" << col << " " << msg << "\n";
        };

        lex::Lexer lx(source, lex_err);
        path::Parser parser(lx, parse_err);
        auto file_prog = parser.parse_program();
        if (!file_prog)
        {
            std::cerr << "Parsing failed for " << p << "\n";
            return nullptr;
        }

        for (auto &d : file_prog->decls)
        {
            if (dynamic_cast<ast::StructDecl *>(d.get()))
                struct_decls.push_back(std::move(d));
            else
                other_decls.push_back(std::move(d));
        }
    }

    for (auto &sd : struct_decls)
        merged->decls.push_back(std::move(sd));
    for (auto &od : other_decls)
        merged->decls.push_back(std::move(od));

    if (debug_ast)
    {
        std::cout << "--- AST (merged) ---\n";
        print_ast(*merged);
    }

    return merged;
}

int main(int argc, char *argv[])
{
    using namespace lex;
    using namespace path;

    if (argc < 2)
    {
        print_help(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    bool emit_ir_only = false;
    bool debug = false;
    fs::path output_dir = ".";

    std::vector<std::string> inputs;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " requires a folder path\n";
                return 1;
            }
            output_dir = argv[i + 1];
            i++;
        }
        else if (arg == "help")
        {
            print_help(argv[0]);
            return 0;
        }
        else if (arg == "ll")
        {
            emit_ir_only = true;
        }
        else if (arg == "debug")
        {
            debug = true;
        }
        else
        {
            inputs.push_back(arg);
        }
    }

    if (inputs.empty())
    {
        std::cerr << "No source files specified\n";
        return 1;
    }

    if (!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }

    auto src_files = collect_sources(inputs);
    if (src_files.empty())
    {
        std::cerr << "No .ec source files found.\n";
        return 1;
    }

    auto program = compile_frontend(src_files, debug, debug);
    if (!program)
        return 1;

    codegen::CodeGen cg("ec");
    if (!cg.generate(*program))
    {
        std::cerr << "codegen failed\n";
        return 1;
    }

    if (emit_ir_only || debug)
    {
        std::cout << "--- LLVM IR ---\n";
        cg.dump_llvm_ir();
    }

    fs::path base = src_files.size() == 1 ? src_files[0] : fs::path("merged");
    fs::path out_file = output_dir / (base.stem().string() + ".ll");

    cg.write_ir_to_file(out_file.string());
    std::cout << "Wrote IR to " << out_file << "\n";

    return 0;
}
