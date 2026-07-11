#include "../include/lexer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

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

    if (lastChar == EOF)
      return gettok();
  }

  if (lastChar == EOF)
    return Token::tok_eof;

  int ThisChar = lastChar;
  lastChar = getchar();
  return Token::tok_char;
}

// parsing

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
