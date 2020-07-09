;;; poke-ras-mode.el --- Major mode for editing poke MAP files

;; Copyright (C) 2020 Jose E. Marchesi

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

;; A major mode for editing poke MAP files.

;;; Code:

(define-derived-mode poke-map-mode prog-mode "Poke MAP"
  "Major mode for editing poke MAP files.

\\{poke-map-mode-map}"
  (defconst poke-map-font-lock-keywords
    '(("^#.*$" 0 font-lock-comment-face nil)
      ("^%[%a-zA-Z0-9_]*" 0 font-lock-preprocessor-face nil)
      ("%>" 0 font-lock-preprocessor-face nil)))
  (setq font-lock-defaults
        '(poke-map-font-lock-keywords t nil nil)))
