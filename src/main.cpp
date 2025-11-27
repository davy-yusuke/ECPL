
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/printer.h"
#include "codegen/codegen.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    using namespace lex;
    using namespace path;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <file1.ec> [file2.ec ...] | <dir>\n";
        return 1;
    }

    std::vector<fs::path> src_files;
    if (fs::is_directory(argv[1]) && argc == 2)
    {
        for (auto &it : fs::recursive_directory_iterator(argv[1]))
        {
            if (it.is_regular_file() && it.path().extension() == ".ec")
                src_files.push_back(it.path());
        }
    }
    else
    {
        for (int i = 1; i < argc; ++i)
        {
            fs::path p(argv[i]);
            if (fs::is_directory(p))
            {
                for (auto &it : fs::recursive_directory_iterator(p))
                {
                    if (it.is_regular_file() && it.path().extension() == ".ec")
                        src_files.push_back(it.path());
                }
            }
            else if (fs::is_regular_file(p))
            {
                src_files.push_back(p);
            }
            else
            {
                std::cerr << "No such file/dir: " << p << "\n";
            }
        }
    }

    if (src_files.empty())
    {
        std::cerr << "No .ec source files found.\n";
        return 1;
    }

    std::unique_ptr<ast::Program> merged = std::make_unique<ast::Program>();
    std::vector<std::unique_ptr<ast::Decl>> struct_decls;
    std::vector<std::unique_ptr<ast::Decl>> other_decls;

    for (const auto &p : src_files)
    {
        std::ifstream in(p);
        if (!in)
        {
            std::cerr << "Failed to open: " << p << "\n";
            return 1;
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

        Lexer lx(source, lex_err);

        Parser parser(lx, parse_err);
        auto file_prog = parser.parse_program();
        if (!file_prog)
        {
            std::cerr << "Parsing failed for " << p << "\n";
            return 1;
        }

        for (auto &d : file_prog->decls)
        {
            if (dynamic_cast<ast::StructDecl *>(d.get()))
            {
                struct_decls.push_back(std::move(d));
            }
            else
            {
                other_decls.push_back(std::move(d));
            }
        }
    }

    for (auto &sd : struct_decls)
        merged->decls.push_back(std::move(sd));
    for (auto &od : other_decls)
        merged->decls.push_back(std::move(od));

    print_ast(*merged);

    codegen::CodeGen cg("ec");

    if (!cg.generate(*merged))
    {
        std::cerr << "codegen failed\n";
        return 1;
    }

    cg.dump_llvm_ir();
    cg.write_ir_to_file("out.ll");
    std::cout << "Wrote IR to out.ll\n";
    return 0;
}
