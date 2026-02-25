#include "resolver.h"
#include "json.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace module
{

    std::optional<ProjectConfig> ProjectConfig::load(const std::filesystem::path &path)
    {
        auto json = load_json_file(path.string());
        if (!json)
        {
            return std::nullopt;
        }

        if (!json->is_object())
        {
            std::cerr << "ecpl.json must be an object\n";
            return std::nullopt;
        }

        ProjectConfig config;
        config.name = json->value("name", "unnamed");
        config.version = json->value("version", "0.0.0");
        config.entry = json->value("entry", "main.ec");
        config.output_dir = json->value("output", "build/");

        auto src_val = json->get("src");
        if (src_val)
        {
            if (src_val->is_array())
            {
                for (const auto &s : src_val->as_array())
                {
                    if (s.is_string())
                        config.src_dirs.push_back(s.as_string());
                }
            }
            else if (src_val->is_string())
            {
                config.src_dirs.push_back(src_val->as_string());
            }
        }
        else
        {
            config.src_dirs.push_back("src/");
        }

        auto deps_val = json->get("dependencies");
        if (deps_val && deps_val->is_array())
        {
            for (const auto &dep : deps_val->as_array())
            {
                if (dep.is_string())
                {
                    config.dependencies.push_back(dep.as_string());
                }
            }
        }

        return config;
    }

    ModuleResolver::ModuleResolver() = default;

    void ModuleResolver::set_project_root(const std::filesystem::path &root)
    {
        project_root_ = std::filesystem::canonical(root);
    }

    void ModuleResolver::add_source_dir(const std::filesystem::path &dir)
    {
        source_dirs_.push_back(dir);
    }

    bool ModuleResolver::resolve_all(const std::vector<std::filesystem::path> &sources)
    {
        for (const auto &file : sources)
        {
            if (!parse_file(file))
            {
                return false;
            }
        }

        for (auto &pair : modules_)
        {
            extract_exports(pair.second);
        }

        if (!resolve_imports())
        {
            return false;
        }

        return true;
    }

    bool ModuleResolver::parse_file(const std::filesystem::path &file)
    {
        std::ifstream in(file);
        if (!in.is_open())
        {
            emit_error("Failed to open file: " + file.string());
            return false;
        }

        std::stringstream buf;
        buf << in.rdbuf();
        std::string source = buf.str();

        auto lex_err = [this, &file](int line, int col, const std::string &msg)
        {
            emit_error("[lexer] " + file.string() + ":" + std::to_string(line) + ":" + std::to_string(col) + " " + msg);
        };

        auto parse_err = [this, &file](int line, int col, const std::string &msg)
        {
            emit_error("[parser] " + file.string() + ":" + std::to_string(line) + ":" + std::to_string(col) + " " + msg);
        };

        lex::Lexer lx(source, lex_err);
        path::Parser parser(lx, parse_err);
        auto program = parser.parse_program();

        if (!program)
        {
            emit_error("Failed to parse: " + file.string());
            return false;
        }

        ModuleInfo info;
        info.file_path = file;
        info.program = std::move(program);

        std::string module_name = file.stem().string();
        for (auto &decl : info.program->decls)
        {
            if (auto mod_decl = dynamic_cast<ast::ModuleDecl *>(decl.get()))
            {
                module_name = mod_decl->name;
                break;
            }
        }

        info.module_name = module_name;
        file_to_module_[file.string()] = module_name;

        for (auto &decl : info.program->decls)
        {
            if (auto import_decl = dynamic_cast<ast::ImportDecl *>(decl.get()))
            {
                info.imports.push_back(import_decl->path);
            }
        }

        modules_[module_name] = std::move(info);
        return true;
    }

    void ModuleResolver::extract_exports(ModuleInfo &info)
    {
        for (auto &decl : info.program->decls)
        {
            if (auto fn = dynamic_cast<ast::FuncDecl *>(decl.get()))
            {
                if (fn->is_pub)
                {
                    SymbolInfo sym;
                    sym.name = fn->name;
                    sym.module_path = info.module_name;
                    sym.is_public = true;
                    sym.is_function = true;
                    sym.decl = fn;
                    info.exported_symbols[fn->name] = sym;
                }
            }
            else if (auto st = dynamic_cast<ast::StructDecl *>(decl.get()))
            {
                if (st->is_pub)
                {
                    SymbolInfo sym;
                    sym.name = st->name;
                    sym.module_path = info.module_name;
                    sym.is_public = true;
                    sym.is_struct = true;
                    sym.decl = st;
                    info.exported_symbols[st->name] = sym;
                }
            }
        }
    }

    std::filesystem::path ModuleResolver::resolve_import_path(const std::string &import_path, const std::filesystem::path &from_file)
    {
        if (!import_path.empty() && import_path[0] == '.')
        {
            std::filesystem::path base = from_file.parent_path();
            std::filesystem::path resolved = base / (import_path + ".ec");
            if (std::filesystem::exists(resolved))
            {
                return resolved;
            }
            resolved = base / import_path / "index.ec";
            if (std::filesystem::exists(resolved))
            {
                return resolved;
            }
        }

        for (const auto &src_dir : source_dirs_)
        {
            std::filesystem::path full_dir = project_root_ / src_dir;
            std::filesystem::path resolved = full_dir / (import_path + ".ec");
            if (std::filesystem::exists(resolved))
            {
                return resolved;
            }
            resolved = full_dir / import_path / "index.ec";
            if (std::filesystem::exists(resolved))
            {
                return resolved;
            }
        }

        return {};
    }

    bool ModuleResolver::resolve_imports()
    {
        bool success = true;

        for (auto &pair : modules_)
        {
            for (const auto &import_path : pair.second.imports)
            {
                std::filesystem::path resolved = resolve_import_path(import_path, pair.second.file_path);
                if (resolved.empty())
                {
                    emit_error("Cannot resolve import: " + import_path + " (from " + pair.second.file_path.string() + ")");
                    success = false;
                    continue;
                }

                auto it = file_to_module_.find(resolved.string());
                if (it == file_to_module_.end())
                {
                    if (!parse_file(resolved))
                    {
                        success = false;
                        continue;
                    }
                    extract_exports(modules_[file_to_module_[resolved.string()]]);
                }
            }
        }

        return success;
    }

    std::unique_ptr<ast::Program> ModuleResolver::link_program()
    {
        auto merged = std::make_unique<ast::Program>();

        std::vector<std::unique_ptr<ast::Decl>> struct_decls;
        std::vector<std::unique_ptr<ast::Decl>> func_decls;

        for (auto &pair : modules_)
        {
            for (auto &decl : pair.second.program->decls)
            {
                if (dynamic_cast<ast::ImportDecl *>(decl.get()))
                {
                    continue;
                }

                if (dynamic_cast<ast::ModuleDecl *>(decl.get()))
                {
                    continue;
                }

                if (dynamic_cast<ast::StmtDecl *>(decl.get()))
                {
                    continue;
                }

                if (dynamic_cast<ast::StructDecl *>(decl.get()))
                {
                    struct_decls.push_back(std::move(decl));
                }
                else if (dynamic_cast<ast::FuncDecl *>(decl.get()))
                {
                    func_decls.push_back(std::move(decl));
                }
            }
        }

        for (auto &sd : struct_decls)
            merged->decls.push_back(std::move(sd));
        for (auto &fd : func_decls)
            merged->decls.push_back(std::move(fd));

        return merged;
    }

    const SymbolInfo *ModuleResolver::resolve_symbol(const std::string &name, const std::string &from_module)
    {
        auto it = modules_.find(from_module);
        if (it == modules_.end())
        {
            return nullptr;
        }

        for (const auto &import_path : it->second.imports)
        {
            auto mod_it = modules_.find(import_path);
            if (mod_it != modules_.end())
            {
                auto sym_it = mod_it->second.exported_symbols.find(name);
                if (sym_it != mod_it->second.exported_symbols.end())
                {
                    return &sym_it->second;
                }
            }
        }

        return nullptr;
    }

    void ModuleResolver::emit_error(const std::string &msg)
    {
        errors_.push_back(msg);
        std::cerr << "[module error] " << msg << "\n";
    }

}
