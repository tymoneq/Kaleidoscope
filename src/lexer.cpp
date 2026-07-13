#include "../include/lexer.hpp"
#include "../include/ast.hpp"
#include <llvm/IR/Value.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

std::string IdentifierStr;
char ThisChar;
double NumVal;
std::map<char, int> BinopPrecedence;
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
  }
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

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
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
    auto RHS = ParsePrimary();
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
  if (CurTok != Token::tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

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
  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
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

// external ::= 'extern' prototype
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

void InitializeModule() {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
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
      fprintf(stderr, "Read top-level expression:");
      FnIR->print(errs());
      fprintf(stderr, "\n");

      // Remove the anonymous expression.
      FnIR->eraseFromParent();
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