#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

// The laxer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things

enum class Token {
  tok_eof,
  tok_def,
  tok_extern,
  tok_identifier,
  tok_number,
  tok_char
};

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
      lastChar == getchar();
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
