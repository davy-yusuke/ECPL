#pragma once
#include "../codegen.h"
#include "../common.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Twine.h>

using namespace llvm;
using namespace codegen;

Value *CodeGen::codegen_call(const ast::CallExpr *ce)
{
    if (auto ident = dynamic_cast<const ast::Ident *>(ce->callee.get()))
    {
        if (ident->name == "println")
        {
            return codegen_println_call(ce);
        }
        else if (ident->name == "printf")
        {
            return codegen_printf_call(ce);
        }
        else if (ident->name == "sprintf")
        {
            return codegen_sprintf_call(ce);
        }
        else if (ident->name == "len")
        {
            return codegen_len_call(ce);
        }
        else if (ident->name == "append")
        {
            return codegen_append_call(ce);
        }
        else if (ident->name == "cast")
        {
            return codegen_cast_call(ce);
        }
        else if (ident->name == "new")
        {
            return codegen_new_call(ce);
        }
    }

    Value *calleeVal = codegen_expr(ce->callee.get());
    if (!calleeVal)
        return nullptr;

    Function *F = nullptr;
    if (auto gv = dyn_cast<GlobalValue>(calleeVal))
    {
        F = dyn_cast<Function>(gv);
    }

    if (!F)
    {
        if (auto id = dynamic_cast<const ast::Ident *>(ce->callee.get()))
        {
            auto it = function_protos.find(id->name);
            if (it != function_protos.end())
                F = it->second;
        }
    }

    if (!F)
    {
        error("call to unknown function");
        return nullptr;
    }

    std::vector<Value *> argsV;
    for (auto &a : ce->args)
    {
        Value *argv = codegen_expr(a.get());
        if (!argv)
            return nullptr;
        argsV.push_back(argv);
    }

    CallInst *callInst = builder.CreateCall(F, argsV);
    if (!F->getReturnType()->isVoidTy())
    {
        callInst->setName("calltmp");
        return callInst;
    }

    return nullptr;
}
