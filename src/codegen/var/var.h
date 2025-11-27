#pragma once
#include "../codegen.h"
#include "../common.h"
#include "type.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Intrinsics.h>

using namespace llvm;
using namespace codegen;

std::string CodeGen::resolve_type_name(ast::Type *tp)
{
    int array_depth = 0;
    int pointer_depth = 0;

    while (tp)
    {

        if (auto *nt = dynamic_cast<ast::NamedType *>(tp))
        {
            std::string result = nt->name;

            for (int i = 0; i < array_depth; i++)
                result += "[]";

            for (int i = 0; i < pointer_depth; i++)
                result += "*";

            return result;
        }

        if (auto *at = dynamic_cast<ast::ArrayType *>(tp))
        {
            array_depth++;
            tp = at->elem.get();
            continue;
        }

        if (auto *pt = dynamic_cast<ast::PointerType *>(tp))
        {
            pointer_depth++;
            tp = pt->base.get();
            continue;
        }

        return {};
    }

    return {};
}

static bool is_primitive_or_empty_type(ast::Type *t)
{
    if (!t)
        return true;

    if (auto *nt = dynamic_cast<ast::NamedType *>(t))
    {
        const std::string &nm = nt->name;
        if (nm.empty())
            return true;
        if (nm == "i32" || nm == "f32" || nm == "bool")
            return true;
        return false;
    }

    return false;
}

Value *CodeGen::codegen_vardecl(const ast::VarDecl *vd)
{
    Function *F = builder.GetInsertBlock()->getParent();
    Type *ty = get_int_type();

    ast::Type *tp = vd->type.get();

    std::string t = resolve_type_name(tp);

    std::cout << "VarDecl: " << vd->name << " type=" << t << std::endl;

    if (Type *tx = getLLVMType(t))
    {
        if (tx)
            ty = tx;
    }

    if (vd->init)
    {
        if (auto sl = dynamic_cast<const ast::StructLiteral *>(vd->init.get()))
        {
            llvm::Value *addr = codegen_struct_literal(sl);
            if (!addr)
                return nullptr;
            bind_local(vd->name, t, addr);
            return addr;
        }
        else
        {
            if (auto alit = dynamic_cast<const ast::ArrayLiteral *>(vd->init.get()))
            {
                ast::Type *tp = alit->array_type.get();
                if (!is_primitive_or_empty_type(tp))
                {
                }
            }

            Value *initV = codegen_expr(vd->init.get());
            if (!initV)
                return nullptr;

            Type *it = initV->getType();
            if (it->isFloatingPointTy())
            {
                ty = get_double_type();
                t = "i32";
            }
            else if (it->isPointerTy())
            {
                ty = it;
            }
            else if (it->isIntegerTy())
            {
                ty = get_int_type();
                t = "i32";
            }

            std::cout << "VarDecl init type: ";
            ty->print(llvm::outs());
            std::cout << std::endl;

            Value *alloca = create_entry_alloca(F, ty, vd->name);
            bind_local(vd->name, t, alloca);

            Value *storeVal = initV;
            if (storeVal->getType() != ty)
            {
                if (storeVal->getType()->isFloatingPointTy() && ty->isIntegerTy())
                    storeVal = builder.CreateFPToSI(storeVal, ty);
                else if (storeVal->getType()->isIntegerTy() && ty->isFloatingPointTy())
                    storeVal = builder.CreateSIToFP(storeVal, ty);
                else
                {
                    if (storeVal->getType()->isPointerTy() && ty->isPointerTy() && storeVal->getType() != ty)
                        storeVal = builder.CreateBitCast(storeVal, ty);
                }
            }
            builder.CreateStore(storeVal, alloca);
            return alloca;
        }
    }
    else
    {
        Value *alloca = create_entry_alloca(F, ty, vd->name);
        builder.CreateStore(Constant::getNullValue(ty), alloca);
        bind_local(vd->name, t, alloca);
        return alloca;
    }
}
