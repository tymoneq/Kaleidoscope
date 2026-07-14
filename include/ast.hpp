#pragma once
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include <cassert>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
using namespace llvm;

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {};
  Value *codegen() override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name_) : Name(Name_) {};
  Value *codegen() override;
  std::string getName() const { return Name; }
};

class IfExpresAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExpresAST(std::unique_ptr<ExprAST> Cond_, std::unique_ptr<ExprAST> Then_,
              std::unique_ptr<ExprAST> Else_)
      : Cond(std::move(Cond_)), Then(std::move(Then_)),
        Else(std::move(Else_)) {};
  Value *codegen() override;
};

// ForExprAST - Expression class for for/in
class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &Name_, std::unique_ptr<ExprAST> Start_,
             std::unique_ptr<ExprAST> End_, std::unique_ptr<ExprAST> Step_,
             std::unique_ptr<ExprAST> Body_)
      : VarName(Name_), Start(std::move(Start_)), End(std::move(End_)),
        Step(std::move(Step_)), Body(std::move(Body_)) {};

  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op_, std::unique_ptr<ExprAST> LHS_,
                std::unique_ptr<ExprAST> RHS_)
      : Op(Op_), LHS(std::move(LHS_)), RHS(std::move(RHS_)) {};
  Value *codegen() override;
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee_,
              std::vector<std::unique_ptr<ExprAST>> Args_)
      : Callee(Callee_), Args(std::move(Args_)) {};
  Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;

public:
  PrototypeAST(const std::string &Name_, const std::vector<std::string> &Args_,
               bool IsOperator = false, unsigned Prec = 0)
      : Name(Name_), Args(std::move(Args_)), IsOperator(IsOperator),
        Precedence(Prec) {};

  const std::string &getName() const { return Name; }
  Function *codegen();
  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }

  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned getBinaryPrecedence() const { return Precedence; }
};

class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {};
  Value *codegen() override;
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto_,
              std::unique_ptr<ExprAST> Body_)
      : Proto(std::move(Proto_)), Body(std::move(Body_)) {};
  Function *codegen();
};

class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {};

  Value *codegen() override;
};

std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
Value *LogErrorV(const char *Str);
Function *getFunction(std::string Name);
