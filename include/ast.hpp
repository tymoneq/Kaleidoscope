#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ExprAST {
public:
  virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {};
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name_) : Name(Name_) {};
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op_, std::unique_ptr<ExprAST> LHS_,
                std::unique_ptr<ExprAST> RHS_)
      : Op(Op_), LHS(std::move(LHS_)), RHS(std::move(RHS_)) {};
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee_,
              std::vector<std::unique_ptr<ExprAST>> Args_)
      : Callee(Callee_), Args(std::move(Args_)) {};
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name_, const std::vector<std::string> &Args_)
      : Name(Name_), Args(std::move(Args_)) {};

  const std::string &getName() const { return Name; }
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto_,
              std::unique_ptr<ExprAST> Body_)
      : Proto(std::move(Proto_)), Body(std::move(Body_)) {};
};
