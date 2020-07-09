;;; poke-ras-mode.el --- Major mode for editing poke RAS files

;; Copyright (C) 2019, 2020 Jose E. Marchesi

;; Maintainer: Jose E. marchesi

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

;; A major mode for editing poke RAS files.

;;; Code:

(defface poke-ras-c-literal-face '((t :foreground "brown")) "" :group 'poke-ras-mode)
(defface poke-ras-variable-face '((t :foreground "green")) "" :group 'poke-ras-mode)
(defface poke-ras-argument-face '((t :foreground "magenta")) "" :group 'poke-ras-mode)

(define-derived-mode poke-ras-mode asm-mode "Poke RAS"
  "Major mode for editing RAS code."
  (font-lock-add-keywords nil
                          '(("^[ \t]*\\.c" . 'poke-ras-c-literal-face)
                            ("[#@][a-zA-Z][0-9a-zA-Z_]+" . 'poke-ras-argument-face)
                            ("\\$[a-zA-Z][0-9a-zA-Z_]*" . 'poke-ras-variable-face)))
  (modify-syntax-entry ?. "w"))

;;; ras-mode.el ends here
