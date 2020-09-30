;;; poke-mode.el --- Major mode for editing Poke programs  -*- lexical-binding: t; -*-

;; Copyright (C) 2020 Aurelien Aptel <aaptel@suse.com>
;; SMIE grammar and help from Stefan Monnier <monnier@iro.umontreal.ca>
;; Version: 0

;; Maintainer: Aurelien Aptel <aaptel@suse.com>

;; This file is NOT part of GNU Emacs.

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

;;; Commentary:

;; A major mode for editing Poke programs.

;; TODO: indentation via smie?
;; TODO: xref source for goto def/list refs
;; TODO: be smarter about types vs identifiers (e.g. offset)
;; TODO: highlight user-defined types/funcs/var/units
;; TODO: comint mode for interactive session?

;;; Code:

(require 'smie)

(defgroup poke nil
  "Poke PK (pickle) editiong mode."
  :group 'languages)

(defface poke-unit
  '((t (:inherit font-lock-constant-face)))
  "Face used to highlight units (#unit).")

(defface poke-attribute
  '((t (:inherit font-lock-builtin-face)))
  "Face used to highlight attributes (var'attribute).")

(defface poke-type
  '((t (:inherit font-lock-type-face)))
  "Face used to highlight builtin types.")

(defface poke-function
  '((t (:inherit font-lock-function-name-face)))
  "Face used to highlight builtin functions.")

(defface poke-constant
  '((t (:inherit font-lock-constant-face)))
  "Face used to highlight builtin constants.")

(defface poke-exception
  '((t (:inherit error)))
  "Face used to highlight builtin exceptions.")

;; from libpoke/pkl-lex.l
(defconst poke-keywords
  '("pinned" "struct" "union" "else" "while" "until" "for" "in" "where" "if"
    "sizeof" "defun" "method" "deftype" "defvar" "defunit" "break" "return"
    "as" "try" "catch" "raise" "any" "print" "printf" "isa"
    "unmap" "big" "little" "load")
  "List of the main keywords of the Poke language.")

;; from libpoke/pkl-lex.l
;; from perl -nE 'say qq{"$1"} if /^deftype (\S+)/' libpoke/*.pk
(defconst poke-builtin-types
  '("string" "void" "int" "uint" "offset"
    "Exception"
    "bit" "nibble" "byte" "char"
    "ushort" "short"
    "ulong" "long"
    "uint8" "uint16" "uint32" "uint64"
    "int8" "int16" "int32" "int64"
    "off64" "uoff64"
    "Comparator"
    "POSIX_Time32" "POSIX_Time64")
  "List of Poke builtin types.")

;; from perl -nE 'say qq{"$1"} if /^defun (\S+)/ && $1 !~ /_pkl/' libpoke/*.pk
(defconst poke-builtin-functions
  '("rand" "get_endian" "set_endian" "get_ios" "set_ios" "open"
    "close" "iosize" "getenv" "exit"
    "catos" "stoca" "atoi" "strchr" "ltrim" "rtrim" "qsort"
    "crc32" "ptime")
  "List of Poke builtin functions.")

;; from perl -nE 'say qq{"$1"} if /^defvar (\S+)/ && $1 !~ /^EC?_/' libpoke/*.pk
(defconst poke-builtin-constants
  '("ENDIAN_LITTLE" "ENDIAN_BIG"
    "IOS_F_READ" "IOS_F_WRITE" "IOS_F_TRUNCATE" "IOS_F_CREATE"
    "IOS_M_RDONLY" "IOS_M_WRONLY" "IOS_M_RDWR"
    "load_path" "NULL")
  "List of Poke builtin constants and variables.")

;; from perl -nE 'say qq{"$1"} if /defvar (EC?_\S+)/' libpoke/*.pk
(defconst poke-builtin-exceptions
  '("EC_generic" "EC_div_by_zero" "EC_no_ios" "EC_no_return" "EC_out_of_bounds"
    "EC_map_bounds" "EC_eof" "EC_map" "EC_conv" "EC_elem" "EC_constraint"
    "EC_io" "EC_signal"  "EC_io_flags" "EC_inval" "EC_exit" "E_generic"
    "E_div_by_zero" "E_no_ios" "E_no_return" "E_out_of_bounds" "E_map_bounds"
    "E_eof" "E_map" "E_conv" "E_elem" "E_constraint" "E_io" "E_signal"
    "E_io_flags" "E_inval" "E_exit")
  "List of Poke builtin exceptions.")

(defvar poke-mode-map
  (let ((map (make-sparse-keymap)))
    map)
  "Keymap used in `poke-mode'.")

(defvar poke-mode-syntax-table
  ;; FIXME: Try and recognize <...> as parens, via `syntax-propertize'.
  (let ((st (make-syntax-table)))
    ;; symbol
    (modify-syntax-entry ?_  "_" st)
    ;; escape
    (modify-syntax-entry ?\\ "\\" st)
    ;; punctuation
    (modify-syntax-entry ?+  "." st)
    (modify-syntax-entry ?-  "." st)
    (modify-syntax-entry ?=  "." st)
    (modify-syntax-entry ?%  "." st)
    (modify-syntax-entry ?<  "." st)
    (modify-syntax-entry ?>  "." st)
    (modify-syntax-entry ?&  "." st)
    (modify-syntax-entry ?|  "." st)
    (modify-syntax-entry ?@  "." st)
    (modify-syntax-entry ?\240 "." st)
    ;; string
    (modify-syntax-entry ?\" "\"" st)
    ;; // and /* */ comments
    (modify-syntax-entry ?\/ ". 124b" st)
    (modify-syntax-entry ?\* ". 23" st)
    ;; balenced punct {} []
    ;; (parens are already done from inherited standard syntax)
    (modify-syntax-entry ?\{ "(}" st)
    (modify-syntax-entry ?\} "){" st)
    (modify-syntax-entry ?\[ "(]" st)
    (modify-syntax-entry ?\] ")[" st)
    st))

(defconst poke-font-lock-keywords
  (list
   ;; shebang
   `(,(rx bol "#!" (* nonl) eol)
     0 'font-lock-comment-face)
   ;; units
   `(,(rx "#" (group (or (+ (any "_0123456789"))
			 (: "0b" (+ (any "_01")))
			 (: "0o" (+ (any "_01234567")))
			 (: "0x" (+ (any "_0123456789abdcdefABCDEF")))
			 (: (any "A-Z" "a-z" "_") (* (any "A-Z" "a-z" "0-9" "_"))))))
     0 'poke-unit)
   ;; char
   `(,(rx "'" (or nonl
		  (: "\\" (any "ntr\\"))
		  (: "\\" (repeat 1 3 (any "01234567"))))
	  "'")
     0 'font-lock-string-face)
   ;; attributes
   `(,(rx "'" (group (any "A-Z" "a-z" "_") (* (any "A-Z" "a-z" "0-9" "_"))))
     0 'poke-attribute)
   ;; keywords
   `(,(rx symbol-start (regexp (regexp-opt poke-keywords)) symbol-end)
     0 'font-lock-keyword-face)
   ;; builtin types
   `(,(rx symbol-start (regexp (regexp-opt poke-builtin-types)) symbol-end)
     0 'poke-type)
   ;; builtin functions
   `(,(rx symbol-start (regexp (regexp-opt poke-builtin-functions)) symbol-end)
     0 'poke-function)
   ;; builtin constants
   `(,(rx symbol-start (regexp (regexp-opt poke-builtin-constants)) symbol-end)
     0 'poke-constant)
   ;; builtin exceptions
   `(,(rx symbol-start (regexp (regexp-opt poke-builtin-exceptions)) symbol-end)
     0 'poke-exception)))

;;;; SMIE (indentation and navigation)

(defcustom poke-indent-basic 2
  "Basic indentation step."
  :type 'integer)

(defvar poke-smie-grammar
  (smie-prec2->grammar
   (smie-bnf->prec2
    '((id)
      (exp ("[" exps "]"))
      (exps (exps "," exps) (exp))
      (definition
        (id "=" inst)
        (id "=" id ":" inst))
      (decl
       ("defvar" definition)
       ("method" definition)
       ("deftype" definition)
       (decl ";" decl))
      (inst
       (id)
       ("struct" id)
       ("union" id)
       ))
    '((assoc ";") (assoc ",")))))

(defun poke--smie-forward-token ()
  ;; FIXME:
  ;; Don't merge ":" or ";" with some preceding punctuation such as ">".
  (smie-default-forward-token))

(defun poke--smie-backward-token ()
  (forward-comment (- (point)))
  (cond
   ;; Don't merge ":" or ";" with some preceding punctuation such as ">".
   ((memq (char-before) '(?: ?\;))
    (forward-char -1)
    (string (char-after)))
   (t (smie-default-backward-token))))

(defun poke-smie-rules (token kind)
  (pcase (cons token kind)
    (`(:elem . basic) poke-indent-basic)
    ;; (`(:list-intro . "=") t)
    ((and `(:before . "{") (guard (smie-rule-parent-p "struct")))
     (smie-rule-parent 0))))

;;;###autoload (add-to-list 'auto-mode-alist '("\\.pk\\'" . poke-mode))
;;;###autoload
(define-derived-mode poke-mode prog-mode "Poke"
  "Major mode for editing Poke programs."

  ;; for comment-region
  (setq-local comment-start "/*")
  (setq-local comment-start-skip "/\\*+[ \t]*")
  (setq-local comment-end "*/")
  (setq-local comment-end-skip "[ \t]*\\*+/")

  (smie-setup poke-smie-grammar #'poke-smie-rules
              :forward-token #'poke--smie-forward-token
              :backward-token #'poke--smie-backward-token)

  ;; font-lock
  (setq-local font-lock-defaults
              '(poke-font-lock-keywords
	        nil ;; do string and comment font-lock from syntax table
	        nil ;; case-sensitive
	        nil)))

(provide 'poke-mode)
;;; poke-mode.el ends here
