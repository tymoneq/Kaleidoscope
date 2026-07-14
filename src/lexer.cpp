#include "../include/lexer.hpp"
#include "../include/ast.hpp"
#include "../include/llvmStructs.hpp"
#include "../include/optimizer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Error.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

std::string IdentifierStr;
char ThisChar;
double NumVal;
Token CurTok;

// gettok - Return the next token from standard input
Token gettok() {
  static char lastChar = ' ';
  while (isspace(lastChar)) {
    lastChar = getchar();
  }

  if (isalpha(lastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = lastChar;
    while (isalnum(lastChar = getchar()))
      IdentifierStr += lastChar;

    if (IdentifierStr == "def")
      return Token::tok_def;
    else if (IdentifierStr == "extern")
      return Token::tok_extern;
    else if (IdentifierStr == "if")
      return Token::tok_if;
    else if (IdentifierStr == "else")
      return Token::tok_else;
    else if (IdentifierStr == "then")
      return Token::tok_then;
    else if (IdentifierStr == "for")
      return Token::tok_for;
    else if (IdentifierStr == "in")
      return Token::tok_in;
    else if (IdentifierStr == "binary")
      return Token::tok_binary;
    else if (IdentifierStr == "unary")
      return Token::tok_unary;
    else if (IdentifierStr == "var")
      return Token::tok_var;
    return Token::tok_identifier;
  }

  if (isdigit(lastChar) || lastChar == '.') { // Number: [0-9.]+
    std::string Numstr;
    do {
      Numstr += lastChar;
      lastChar = getchar();
    } while (isdigit(lastChar) || lastChar == '.');
    NumVal =
        strtod(Numstr.c_str(), 0); // function returns incorrect results
                                   // for 1.23.45 to do improve to better one
    return Token::tok_number;
  }

  if (lastChar == '#') {
    do
      lastChar = getchar();
    while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');

    if (lastChar != EOF)
      return gettok();
  }

  if (lastChar == EOF)
    return Token::tok_eof;

  ThisChar = lastChar;
  lastChar = getchar();
  return Token::tok_char;
}

// parsing
//------------------------------------------------//
//------------------------------------------------//
//------------------------------------------------//

Token getNextToken() { return CurTok = gettok(); }

// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Results = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();
  return std::move(Results);
}

static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS);

// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParentExpr() {
  getNextToken();
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok == Token::tok_char && ThisChar != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}

// identifierexpr
// ::= identifier
// ::= identifier '(' expresion* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken();

  if (CurTok != Token::tok_char ||
      (CurTok == Token::tok_char && ThisChar != '('))
    return std::make_unique<VariableExprAST>(IdName);

  getNextToken();
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (ThisChar != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (ThisChar == ')')
        break;

      if (ThisChar != ',')
        return LogError("Expected ')' or ',' in argument list");

      getNextToken();
    }
  }

  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// ifexpr ::= "if" expression 'then' expression 'else' expression
static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken();
  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != Token::tok_then)
    return LogError("expected then");

  getNextToken();

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != Token::tok_else)
    return LogError("Expected else");

  getNextToken();
  auto Else = ParseExpression();

  if (!Else)
    return nullptr;

  return std::make_unique<IfExpresAST>(std::move(Cond), std::move(Then),
                                       std::move(Else));
}

// forexpr ::= for identifier = expr, in expression
static std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken();

  if (CurTok != Token::tok_identifier)
    return LogError("expected identifier after for");

  std::string IdName = IdentifierStr;
  getNextToken();

  if (CurTok != Token::tok_char ||
      (CurTok == Token::tok_char && ThisChar != '='))
    return LogError("expected '=' after for");

  getNextToken();

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != Token::tok_char ||
      (CurTok == Token::tok_char && ThisChar != ','))
    return LogError("expected ',' after for start value");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  std::unique_ptr<ExprAST> Step;
  if (CurTok == Token::tok_char && ThisChar == ',') {
    getNextToken();
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != Token::tok_in)
    return LogError("expected 'in' after for");
  getNextToken();

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                      std::move(Step), std::move(Body));
}

static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken();
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  if (CurTok != Token::tok_identifier)
    return LogError("Expected identifier after var");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken();

    std::unique_ptr<ExprAST> Init;
    if (CurTok == Token::tok_char && ThisChar == '=') {
      getNextToken();
      Init = ParseExpression();
      if (!Init)
        return nullptr;
    }
    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    if (CurTok != Token::tok_char)
      break;
    getNextToken();

    if (CurTok != Token::tok_identifier)
      return LogError("expected identifier list after var");
  }
  if (CurTok != Token::tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken();

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  if (CurTok == Token::tok_identifier) {
    return ParseIdentifierExpr();
  } else if (CurTok == Token::tok_number) {
    return ParseNumberExpr();
  } else if (CurTok == Token::tok_char && ThisChar == '(') {
    return ParseParentExpr();
  } else if (CurTok == Token::tok_if)
    return ParseIfExpr();
  else if (CurTok == Token::tok_for)
    return ParseForExpr();
  else if (CurTok == Token::tok_var)
    return ParseVarExpr();
  return LogError("unknown token when expecting an expression");
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.

// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (CurTok != Token::tok_char)
    return -1;

  // Make sure it's a declared binoperator
  int TokPrec = BinopPrecedence[ThisChar];
  if (TokPrec <= 0)
    return -1;

  return TokPrec;
}
// unary
// ::= primary
// ::= `!` unary
static std::unique_ptr<ExprAST> ParseUnary() {
  if (CurTok != Token::tok_char ||
      (CurTok == Token::tok_char && (ThisChar == '(' || ThisChar == ',')))
    return ParsePrimary();

  char Opc = ThisChar;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;
  return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();
    if (TokPrec < ExprPrec)
      return LHS;

    char BinOp = ThisChar;
    getNextToken();
    auto RHS = ParseUnary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName = IdentifierStr;
  unsigned Kind = 0;
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case Token::tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;

  case Token::tok_unary:
    getNextToken();
    if (!isascii(ThisChar))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)ThisChar;
    Kind = 1;
    getNextToken();
    break;

  case Token::tok_binary:
    getNextToken();
    if (!isascii(ThisChar))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += char(ThisChar);
    Kind = 2;
    getNextToken();

    if (CurTok == Token::tok_number) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  }

  if (CurTok == Token::tok_char && ThisChar != '(')
    return LogErrorP("Exprected '(' in prototype");

  // Read the list of argument names
  std::vector<std::string> ArgNames;
  while (getNextToken() == Token::tok_identifier) {
    ArgNames.push_back(IdentifierStr);
  }
  if (CurTok == Token::tok_char && ThisChar != ')')
    return LogErrorP("Expected ')' in prototype");

  getNextToken();
  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0,
                                        BinaryPrecedence);
}
/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));

  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();
  return ParsePrototype();
}

// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      ExitOnErr(TheJIT->addModule(
          ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
      InitializeModuleAndManagers();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      auto RT = TheJIT->getMainJITDylib().createResourceTracker();
      auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
      InitializeModuleAndManagers();

      auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
      double (*FP)() = ExprSymbol.toPtr<double (*)()>();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      ExitOnErr(RT->remove());
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

void MainLoop() {

  while (true) {
    fprintf(stderr, "ready> ");
    if (CurTok == Token::tok_eof)
      return;
    else if (CurTok == Token::tok_char && ThisChar == ';') {
      getNextToken();
    } else if (CurTok == Token::tok_def) {
      HandleDefinition();
    } else if (CurTok == Token::tok_extern) {
      HandleExtern();
    } else {
      HandleTopLevelExpression();
    }
  }
}
