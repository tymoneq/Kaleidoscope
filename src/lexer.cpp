#include "../include/lexer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static std::string IdentifierStr;
static int ThisChar;
static double NumVal;

// gettok - Return the next token from standard input
static Token gettok() {
  static int lastChar = ' ';
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

  int ThisChar = lastChar;
  lastChar = getchar();
  return Token::tok_char;
}

/////////////////////////////////////////////////////////////
// parsing
//
/////////////////////////////////////////////////////////////

static Token CurTok;
static Token getNextToken() { return CurTok = gettok(); }

// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

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

  if (CurTok == Token::tok_char && ThisChar != '(')
    return std::make_unique<VariableExprAST>(IdName);

  getNextToken();

  std::vector<std::unique_ptr<ExprAST>> Args;

  if (CurTok == Token::tok_char && ThisChar != ')') {
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
  } else if (CurTok == Token::tok_char) {
    if (ThisChar == '(')
      return ParseParentExpr();
  } else {
    return LogError("unknown token when expecting an expression");
  }
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.

static std::map<char, int> BinopPrecedence;

// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (CurTok == Token::tok_char)
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

    int BinOp = ThisChar;
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

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.
}
