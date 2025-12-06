# OurScheme — 以 C++ 實作的 Scheme interpreter(直譯器)
A Scheme interpreter implemented in modern C++, featuring lexical analysis, recursive-descent parsing, AST evaluation, lexical scoping, lambda closures, and error-object semantics.

🔹功能特色（Features）
語言基本功能

  數字（int / float）

  字串（string）

  符號（symbol）

  布林值（#t / #f）

  cons、car、cdr、list

  quote、if、cond

  一般函式呼叫：(+ 1 2)、(f x y) 等

🔹高階語意

  lexical scoping（靜態作用域）

  lambda （closure）

  使用者自定義函式

  define（變數/函式定義）

  set!（變數重新綁定）

  eval（動態求值）

🔹Project 4 進階功能

  create-error-object

  error-object?

  read、write

  display-string、newline

  symbol->string、number->string

  Verbose 模式（方便除錯與觀察 AST）

🔹系統架構亮點

  採用 std::shared_ptr 管理 AST，避免記憶體洩漏或重複釋放

  完整處理 improper list、quote 結構、nested S-expression

  具備安全 clone 機制（SafeClone），避免環境錯誤或 AST 破壞
  
  錯誤物件機制確保求值流程穩定性
      |        REPL        | -> |       Lexer        | -> |      Parser        | -> |  (AST Builder)     | -> |      Evaluator     | -> |       Result       |


🔹求值流程（Evaluation Flow）
  Lexer
    將輸入字串轉換為 Token（括號、符號、字串、數字、布林、dot 等）。

  Parser
    以遞迴下降法解析 Token 序列，生成 AST。

  Evaluator
    依語意規則求值：
      Atom → 直接回傳或查變數表
      List → 分成 special form 或 procedure call
      Lambda → 建立 closure
      Function call → 建立新環境並依序求值 body
