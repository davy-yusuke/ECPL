#pragma once
#include "../codegen.h"
#include "../common.h"
#include "../array/parse.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <stdexcept>

llvm::Type *CodeGen::getLLVMType(const std::string &typeName)
{
    using namespace llvm;

    ParsedType pt = parse_type_chain(typeName);

    Type *ty = nullptr;

    if (pt.base == "i32")
    {
        ty = Type::getInt32Ty(context);
    }
    else if (pt.base == "i64")
    {
        ty = Type::getInt64Ty(context);
    }
    else if (pt.base == "f32")
    {
        ty = Type::getFloatTy(context);
    }
    else if (pt.base == "f64")
    {
        ty = Type::getDoubleTy(context);
    }
    else if (pt.base == "string")
    {
        ty = codegen::detail::getI8PtrTy(context);
    }
    else if (pt.base == "byte")
    {
        ty = Type::getInt8Ty(context);
    }
    else
    {
        std::cout << "Looking up type: " << pt.base << std::endl;
        ty = lookup_struct_type(pt.base);
        if (!ty)
        {
            throw std::runtime_error("Unknown type: " + pt.base);
        }
    }

    return ty;
}
