#pragma once
#include "../codegen.h"
#include "../common.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Intrinsics.h>

using namespace llvm;
using namespace codegen;

Value *CodeGen::codegen_binary(const ast::BinaryExpr *be)
{
    if (!be)
        return nullptr;
    Value *L = codegen_expr(be->left.get());
    Value *R = codegen_expr(be->right.get());
    if (!L || !R)
        return nullptr;

    bool is_fp = L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy();
    if (is_fp)
    {
        if (!L->getType()->isFloatingPointTy())
            L = builder.CreateSIToFP(L, get_double_type(), "sitofp_l");
        if (!R->getType()->isFloatingPointTy())
            R = builder.CreateSIToFP(R, get_double_type(), "sitofp_r");

        if (be->op == "/" || be->op == "%")
        {

            Value *isZero = builder.CreateFCmpUEQ(R, ConstantFP::get(get_double_type(), 0.0), "div_zero_cmp");
            Function *F = builder.GetInsertBlock()->getParent();
            BasicBlock *okBB = BasicBlock::Create(builder.getContext(), "div_ok", F);
            BasicBlock *badBB = BasicBlock::Create(builder.getContext(), "div_by_zero", F);

            builder.CreateCondBr(isZero, badBB, okBB);

            builder.SetInsertPoint(badBB);
            {
                FunctionType *abortTy = FunctionType::get(Type::getVoidTy(builder.getContext()), {}, false);
                FunctionCallee abortFn = module->getOrInsertFunction("abort", abortTy);
                builder.CreateCall(abortFn, {});
                builder.CreateUnreachable();
            }

            builder.SetInsertPoint(okBB);
        }

        Type *type = L->getType();
        if (type->isFloatingPointTy())
        {
            if (be->op == "+")
                return builder.CreateFAdd(L, R, "addtmp");
            if (be->op == "-")
                return builder.CreateFSub(L, R, "subtmp");
            if (be->op == "*")
                return builder.CreateFMul(L, R, "multmp");
            if (be->op == "/")
                return builder.CreateFDiv(L, R, "divtmp");
            if (be->op == "%")
                return builder.CreateFRem(L, R, "remtmp");
        }
        else if (type->isIntegerTy())
        {

            llvm::Type *targetType = nullptr;
            if (L->getType()->getIntegerBitWidth() >= R->getType()->getIntegerBitWidth())
                targetType = L->getType();
            else
                targetType = R->getType();

            L = castToSameIntType(L, targetType);
            R = castToSameIntType(R, targetType);

            if (be->op == "+")
                return builder.CreateAdd(L, R, "addtmp");
            if (be->op == "-")
                return builder.CreateSub(L, R, "subtmp");
            if (be->op == "*")
                return builder.CreateMul(L, R, "multmp");
            if (be->op == "/")
                return builder.CreateSDiv(L, R, "divtmp");
            if (be->op == "%")
                return builder.CreateSRem(L, R, "remtmp");

            if (be->op == "<<")
                return builder.CreateShl(L, R, "shltmp");
            if (be->op == ">>")

                return builder.CreateAShr(L, R, "shrtmp");
        }
        else if (type->isPointerTy())
        {
            if (be->op == "+")
            {
                if (R->getType()->isIntegerTy())
                    return builder.CreateGEP(type, L, R, "ptraddtmp");
                else
                    error("pointer addition requires integer");
            }
            else if (be->op == "-")
            {
                if (R->getType()->isIntegerTy())
                    return builder.CreateGEP(type, L, builder.CreateNeg(R), "ptrsubtmp");
                else
                    error("pointer subtraction requires integer");
            }
        }
        else
        {
            error("unsupported operand type for binary operator");
        }

        if (be->op == ">")
            return builder.CreateZExt(builder.CreateFCmpUGT(L, R, "cmptmp"), get_int_type());
        if (be->op == "<")
            return builder.CreateZExt(builder.CreateFCmpULT(L, R, "cmptmp"), get_int_type());
        if (be->op == ">=")
            return builder.CreateZExt(builder.CreateFCmpUGE(L, R, "cmptmp"), get_int_type());
        if (be->op == "<=")
            return builder.CreateZExt(builder.CreateFCmpULE(L, R, "cmptmp"), get_int_type());
    }
    else
    {

        if (be->op == "/" || be->op == "%")
        {
            Value *zero = ConstantInt::get(get_int_type(), 0);
            Value *isZero = builder.CreateICmpEQ(R, zero, "div_zero_cmp_int");

            Function *F = builder.GetInsertBlock()->getParent();
            BasicBlock *okBB = BasicBlock::Create(builder.getContext(), "div_ok", F);
            BasicBlock *badBB = BasicBlock::Create(builder.getContext(), "div_by_zero", F);

            builder.CreateCondBr(isZero, badBB, okBB);

            builder.SetInsertPoint(badBB);
            {
                FunctionType *abortTy = FunctionType::get(Type::getVoidTy(builder.getContext()), {}, false);
                FunctionCallee abortFn = module->getOrInsertFunction("abort", abortTy);
                builder.CreateCall(abortFn, {});
                builder.CreateUnreachable();
            }

            builder.SetInsertPoint(okBB);
        }

        Type *type = L->getType();

        if (type->isFloatingPointTy())
        {
            if (be->op == "+")
                return builder.CreateFAdd(L, R, "addtmp");
            if (be->op == "-")
                return builder.CreateFSub(L, R, "subtmp");
            if (be->op == "*")
                return builder.CreateFMul(L, R, "multmp");
            if (be->op == "/")
                return builder.CreateFDiv(L, R, "divtmp");
            if (be->op == "%")
                return builder.CreateFRem(L, R, "remtmp");
        }
        else if (type->isIntegerTy())
        {

            llvm::Type *targetType = nullptr;
            if (L->getType()->getIntegerBitWidth() >= R->getType()->getIntegerBitWidth())
            {
                targetType = L->getType();
            }
            else
            {
                targetType = R->getType();
            }

            L = castToSameIntType(L, targetType);
            R = castToSameIntType(R, targetType);

            if (be->op == "+")
                return builder.CreateAdd(L, R, "addtmp");
            if (be->op == "-")
                return builder.CreateSub(L, R, "subtmp");
            if (be->op == "*")
                return builder.CreateMul(L, R, "multmp");
            if (be->op == "/")
                return builder.CreateSDiv(L, R, "divtmp");
            if (be->op == "%")
                return builder.CreateSRem(L, R, "remtmp");

            if (be->op == "<<")
                return builder.CreateShl(L, R, "shltmp");
            if (be->op == ">>")
                return builder.CreateAShr(L, R, "shrtmp");
        }
        else if (type->isPointerTy())
        {
            if (be->op == "+")
            {
                if (R->getType()->isIntegerTy())
                    return builder.CreateGEP(builder.getInt8Ty(), L, R, "ptraddtmp");
                else
                    error("pointer addition requires integer");
            }
            else if (be->op == "-")
            {
                if (R->getType()->isIntegerTy())
                    return builder.CreateGEP(builder.getInt8Ty(), L, builder.CreateNeg(R), "ptrsubtmp");
                else
                    error("pointer subtraction requires integer");
            }
        }
        else
        {
            error("unsupported operand type for binary operator");
        }

        if (L->getType()->isPointerTy() || R->getType()->isPointerTy())
        {
            Value *nullPtr = ConstantPointerNull::get(cast<PointerType>(L->getType()));
            if (be->op == "==")
                return builder.CreateZExt(builder.CreateICmpEQ(L, nullPtr, "cmptmp"), get_int_type());
            if (be->op == "!=")
                return builder.CreateZExt(builder.CreateICmpNE(L, nullPtr, "cmptmp"), get_int_type());
            error("unsupported pointer comparison for " + be->op);
        }
        else
        {
            llvm::Type *targetType = nullptr;

            if (L->getType()->getIntegerBitWidth() >= R->getType()->getIntegerBitWidth())
            {
                targetType = L->getType();
            }
            else
            {
                targetType = R->getType();
            }

            L = castToSameIntType(L, targetType);
            R = castToSameIntType(R, targetType);

            if (be->op == ">")
                return builder.CreateZExt(builder.CreateICmpSGT(L, R, "cmptmp"), get_int_type());
            if (be->op == "<")
                return builder.CreateZExt(builder.CreateICmpSLT(L, R, "cmptmp"), get_int_type());
            if (be->op == ">=")
                return builder.CreateZExt(builder.CreateICmpSGE(L, R, "cmptmp"), get_int_type());
            if (be->op == "<=")
                return builder.CreateZExt(builder.CreateICmpSLE(L, R, "cmptmp"), get_int_type());
            if (be->op == "==")
                return builder.CreateZExt(builder.CreateICmpEQ(L, R, "cmptmp"), get_int_type());
            if (be->op == "!=")
                return builder.CreateZExt(builder.CreateICmpNE(L, R, "cmptmp"), get_int_type());
        }
    }

    if (be->op == "&&" || be->op == "||")
    {
        auto Lbool = builder.CreateICmpNE(L, ConstantInt::get(get_int_type(), 0), "lhsbool");
        auto Rbool = builder.CreateICmpNE(R, ConstantInt::get(get_int_type(), 0), "rhsbool");
        if (be->op == "&&")
        {
            auto andv = builder.CreateAnd(Lbool, Rbool, "andtmp");
            return builder.CreateZExt(andv, get_int_type());
        }
        else
        {
            auto orv = builder.CreateOr(Lbool, Rbool, "ortmp");
            return builder.CreateZExt(orv, get_int_type());
        }
    }

    error("unsupported binary op: " + be->op);
    return nullptr;
}
