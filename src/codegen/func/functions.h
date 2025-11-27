#pragma once
#include "../codegen.h"
#include "../common.h"
#include "../array/parse.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Intrinsics.h>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace codegen;

llvm::Type *CodeGen::resolve_type_from_ast_local(const ast::Type *astType)
{
    if (!astType)
        return nullptr;

    if (auto namedType = dynamic_cast<const ast::NamedType *>(astType))
    {
        return resolve_type_by_name(namedType->name);
    }

    if (auto pointerType = dynamic_cast<const ast::PointerType *>(astType))
    {
        llvm::Type *innerType = resolve_type_from_ast_local(pointerType->base.get());
        if (!innerType)
            innerType = get_int_type();
        return llvm::PointerType::getUnqual(innerType);
    }

    if (auto arrayType = dynamic_cast<const ast::ArrayType *>(astType))
    {
        llvm::Type *elementType = resolve_type_from_ast_local(arrayType->elem.get());
        if (!elementType)
            elementType = get_int_type();

        return llvm::PointerType::getUnqual(elementType);
    }

    if (auto funcTypeAst = dynamic_cast<const ast::FuncType *>(astType))
    {
        std::vector<llvm::Type *> paramTypes;
        for (const auto &param : funcTypeAst->params)
        {
            llvm::Type *paramType = resolve_type_from_ast_local(param.get());
            if (!paramType)
                paramType = get_int_type();
            paramTypes.push_back(paramType);
        }

        llvm::Type *returnType = nullptr;
        if (funcTypeAst->ret)
            returnType = resolve_type_from_ast_local(funcTypeAst->ret.get());
        if (!returnType)
            returnType = get_int_type();

        llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, /*isVarArg=*/false);
        return llvm::PointerType::getUnqual(functionType);
    }

    return nullptr;
}

void CodeGen::predeclare_functions(const std::vector<const ast::FuncDecl *> &funcDecls)
{
    register_builtin_ffi();

    LLVMContext &context = builder.getContext();

    for (const ast::FuncDecl *funcDecl : funcDecls)
    {
        if (!funcDecl)
            continue;

        if (function_protos.find(funcDecl->name) != function_protos.end())
            continue;

        bool isVarArg = false;
        if (!funcDecl->params.empty() && funcDecl->params.back().variadic)
            isVarArg = true;
        for (size_t i = 0; i + 1 < funcDecl->params.size(); ++i)
        {
            if (funcDecl->params[i].variadic)
            {
                error("variadic parameter must be the last parameter in function: " + funcDecl->name);
                break;
            }
        }

        std::vector<Type *> argTypes;

        for (size_t i = 0; i < funcDecl->params.size(); ++i)
        {
            const auto &param = funcDecl->params[i];
            if (param.variadic)
            {
                continue;
            }

            llvm::Type *paramType = nullptr;
            if (param.type)
                paramType = resolve_type_from_ast_local(param.type.get());
            if (!paramType)
                paramType = get_int_type();
            argTypes.push_back(paramType);
        }

        Type *returnType = get_void_type();
        if (funcDecl->ret_type)
        {
            llvm::Type *rt = resolve_type_from_ast_local(funcDecl->ret_type.get());
            if (rt)
                returnType = rt;
            else
                returnType = get_int_type();
        }

        FunctionType *functionType = FunctionType::get(returnType, argTypes, isVarArg);

        Function *existing = module->getFunction(funcDecl->name);
        if (existing)
        {
            function_protos[funcDecl->name] = existing;
            continue;
        }

        Function *fn = Function::Create(functionType, Function::ExternalLinkage, funcDecl->name, module.get());

        unsigned argIndex = 0;
        for (auto &arg : fn->args())
        {
            if (argIndex < argTypes.size() && argIndex < funcDecl->params.size())
            {
                size_t p = 0;
                unsigned seen = 0;
                for (p = 0; p < funcDecl->params.size(); ++p)
                {
                    if (!funcDecl->params[p].variadic)
                    {
                        if (seen == argIndex)
                            break;
                        ++seen;
                    }
                }
                if (p < funcDecl->params.size())
                {
                    arg.setName(funcDecl->params[p].name);
                }
            }
            ++argIndex;
        }

        function_protos[funcDecl->name] = fn;
    }
}

Function *CodeGen::codegen_function_decl(const ast::FuncDecl *funcDecl)
{
    if (!funcDecl)
        return nullptr;

    LLVMContext &context = builder.getContext();
    Module *M = module.get();

    std::cout << "code generating..." << std::endl;

    bool isVarArg = false;
    if (!funcDecl->params.empty() && funcDecl->params.back().variadic)
        isVarArg = true;
    for (size_t i = 0; i + 1 < funcDecl->params.size(); ++i)
    {
        if (funcDecl->params[i].variadic)
        {
            error("variadic parameter must be the last parameter in function: " + funcDecl->name);

            break;
        }
    }

    std::vector<Type *> argTypes;

    for (size_t i = 0; i < funcDecl->params.size(); ++i)
    {
        const auto &param = funcDecl->params[i];
        if (param.variadic)
            continue;
        llvm::Type *paramType = nullptr;
        if (param.type)
            paramType = resolve_type_from_ast_local(param.type.get());
        if (!paramType)
            paramType = get_int_type();
        argTypes.push_back(paramType);
    }

    Type *returnType = get_void_type();
    if (funcDecl->ret_type)
    {
        llvm::Type *rt = resolve_type_from_ast_local(funcDecl->ret_type.get());
        if (rt)
            returnType = rt;
        else
            returnType = get_int_type();
    }

    FunctionType *functionType = FunctionType::get(returnType, argTypes, isVarArg);

    Function *functionValue = module->getFunction(funcDecl->name);
    if (!functionValue)
    {
        functionValue = Function::Create(functionType, Function::ExternalLinkage, funcDecl->name, module.get());
        function_protos[funcDecl->name] = functionValue;
    }
    else
    {
        if (functionValue->getFunctionType() != functionType)
        {
            std::string existingStr, expectedStr;
            {
                llvm::raw_string_ostream os(existingStr);
                functionValue->getFunctionType()->print(os);
            }
            {
                llvm::raw_string_ostream os(expectedStr);
                functionType->print(os);
            }
            error("function declaration/definition type mismatch for: " + funcDecl->name +
                  " decl=" + existingStr + " expected=" + expectedStr);
            return nullptr;
        }

        if (!functionValue->empty())
        {
            error("redefinition of function: " + funcDecl->name);
            return nullptr;
        }
    }

    BasicBlock *entryBlock = BasicBlock::Create(context, "entry", functionValue);
    builder.SetInsertPoint(entryBlock);

    unsigned argIndex = 0;
    IRBuilder<> entryBuilder(entryBlock, entryBlock->begin());

    for (auto &arg : functionValue->args())
    {
        size_t p = 0;
        unsigned seen = 0;
        for (p = 0; p < funcDecl->params.size(); ++p)
        {
            if (!funcDecl->params[p].variadic)
            {
                if (seen == argIndex)
                    break;
                ++seen;
            }
        }

        std::string argName = (p < funcDecl->params.size() ? funcDecl->params[p].name : std::string(arg.getName()));
        arg.setName(argName);

        ast::Type *paramAstType = (p < funcDecl->params.size()) ? funcDecl->params[p].type.get() : nullptr;

        auto pt = parse_type_chain(resolve_type_name(paramAstType));

        if (arg.getType()->isPointerTy())
        {
            bind_local(argName, pt.base + "_params", &arg);
        }
        else
        {
            Value *localAlloca = entryBuilder.CreateAlloca(arg.getType(), nullptr, argName);
            entryBuilder.CreateStore(&arg, localAlloca);
            bind_local(argName, pt.base + "_params", localAlloca);
        }
        ++argIndex;
    }

    if (!funcDecl->params.empty() && funcDecl->params.back().variadic)
    {
        const auto &vparam = funcDecl->params.back();
        llvm::Type *elemType = nullptr;
        if (vparam.type)
            elemType = resolve_type_from_ast_local(vparam.type.get());
        if (!elemType)
            elemType = get_int_type();

        llvm::Type *holderType = llvm::PointerType::getUnqual(elemType);
        Value *varAlloca = entryBuilder.CreateAlloca(holderType, nullptr, vparam.name);

        entryBuilder.CreateStore(Constant::getNullValue(holderType), varAlloca);
        bind_local(vparam.name, "ptr", varAlloca);
    }

    push_scope();

    if (funcDecl->body)
        codegen_block(funcDecl->body.get());

    if (auto currentBB = builder.GetInsertBlock())
    {
        if (!currentBB->getTerminator())
        {
            if (returnType->isVoidTy())
                builder.CreateRetVoid();
            else
                builder.CreateRet(Constant::getNullValue(returnType));
        }
    }

    if (verifyFunction(*functionValue, &errs()))
    {
        error("function verification failed: " + funcDecl->name);
        functionValue->eraseFromParent();
        pop_scope();
        return nullptr;
    }

    pop_scope();
    return functionValue;
}
