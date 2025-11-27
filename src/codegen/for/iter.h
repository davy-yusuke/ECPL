#pragma once
#include "../codegen.h"
#include "../common.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Intrinsics.h>

using namespace llvm;
using namespace codegen;

Value *CodeGen::codegen_forinstmt(const ast::ForInStmt *fs)
{
    Function *F = builder.GetInsertBlock()->getParent();
    Value *iterV = codegen_expr(fs->iterable.get());
    if (!iterV)
        return nullptr;

    if (iterV->getType()->isPointerTy())
    {
        Type *i8Ty = Type::getInt8Ty(context);
        Type *i8ptr = PointerType::get(i8Ty, 0);
        Value *strPtr = iterV;
        if (iterV->getType() != i8ptr)
            strPtr = builder.CreateBitCast(iterV, i8ptr, "strptr_cast");

        Value *idxAlloca = create_entry_alloca(F, get_int_type(), ".forin.idx");
        builder.CreateStore(ConstantInt::get(get_int_type(), 0), idxAlloca);

        BasicBlock *condBB = BasicBlock::Create(context, "forin.cond", F);
        BasicBlock *bodyBB = BasicBlock::Create(context, "forin.body", F);
        BasicBlock *incrBB = BasicBlock::Create(context, "forin.incr", F);
        BasicBlock *afterBB = BasicBlock::Create(context, "forin.end", F);

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBB);

        builder.SetInsertPoint(condBB);
        Value *idxLoad = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load");

        Value *ptr = builder.CreateGEP(i8Ty, strPtr, idxLoad, "forin.gep");
        Value *ch = builder.CreateLoad(i8Ty, ptr, "forin.ch");
        Value *zero8 = ConstantInt::get(Type::getInt8Ty(context), 0);
        Value *cond = builder.CreateICmpNE(ch, zero8, "forin.cond");
        builder.CreateCondBr(cond, bodyBB, afterBB);

        Value *varAlloca = create_entry_alloca(F, get_int_type(), fs->var);

        break_targets.push_back(afterBB);
        continue_targets.push_back(incrBB);

        builder.SetInsertPoint(bodyBB);
        push_scope();

        bind_local(fs->var, "i32", varAlloca);

        Value *idxInBody = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load2");
        Value *ptrInBody = builder.CreateGEP(i8Ty, strPtr, idxInBody, "forin.gep2");
        Value *ch2 = builder.CreateLoad(i8Ty, ptrInBody, "forin.ch2");

        Value *chExt = builder.CreateSExt(ch2, get_int_type(), "forin.ch.ext");
        builder.CreateStore(chExt, varAlloca);

        if (fs->body)
            codegen_block(fs->body.get());

        pop_scope();

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(incrBB);

        builder.SetInsertPoint(incrBB);
        Value *idxOld = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load3");
        Value *one = ConstantInt::get(get_int_type(), 1);
        Value *idxNew = builder.CreateAdd(idxOld, one, ".forin.idx.inc");
        builder.CreateStore(idxNew, idxAlloca);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBB);

        break_targets.pop_back();
        continue_targets.pop_back();

        builder.SetInsertPoint(afterBB);

        return nullptr;
    }

    if (iterV->getType()->isIntegerTy() || iterV->getType()->isFloatingPointTy())
    {

        Value *endVal = iterV;
        if (iterV->getType()->isFloatingPointTy())
        {
            endVal = builder.CreateFPToSI(iterV, get_int_type(), "end_fp_to_i");
        }
        else if (iterV->getType()->isIntegerTy() && !iterV->getType()->isIntegerTy(get_int_type()->getIntegerBitWidth()))
        {
            endVal = builder.CreateSExtOrTrunc(iterV, get_int_type(), "end_sext_trunc");
        }

        Value *idxAlloca = create_entry_alloca(F, get_int_type(), ".forin.idx");
        builder.CreateStore(ConstantInt::get(get_int_type(), 0), idxAlloca);

        BasicBlock *condBB = BasicBlock::Create(context, "forin.cond", F);
        BasicBlock *bodyBB = BasicBlock::Create(context, "forin.body", F);
        BasicBlock *incrBB = BasicBlock::Create(context, "forin.incr", F);
        BasicBlock *afterBB = BasicBlock::Create(context, "forin.end", F);

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBB);

        builder.SetInsertPoint(condBB);
        Value *idxLoad = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load");
        Value *cmp = builder.CreateICmpSLT(idxLoad, endVal, "forin.cmp");
        builder.CreateCondBr(cmp, bodyBB, afterBB);

        Value *varAlloca = create_entry_alloca(F, get_int_type(), fs->var);

        break_targets.push_back(afterBB);
        continue_targets.push_back(incrBB);

        builder.SetInsertPoint(bodyBB);
        push_scope();

        bind_local(fs->var, "i32", varAlloca);

        Value *idxInBody = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load2");
        builder.CreateStore(idxInBody, varAlloca);

        if (fs->body)
            codegen_block(fs->body.get());

        pop_scope();

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(incrBB);

        builder.SetInsertPoint(incrBB);
        Value *idxOld = builder.CreateLoad(get_int_type(), idxAlloca, ".forin.idx.load3");
        Value *one = ConstantInt::get(get_int_type(), 1);
        Value *idxNew = builder.CreateAdd(idxOld, one, ".forin.idx.inc");
        builder.CreateStore(idxNew, idxAlloca);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBB);

        break_targets.pop_back();
        continue_targets.pop_back();

        builder.SetInsertPoint(afterBB);
        return nullptr;
    }

    error("for-in only supports string (i8*), integer, or floating iterable for now");
    return nullptr;
}
