;; poke-gdb.scm -- GDB extensions for debugging GNU poke

;; Copyright (C) 2019, 2020 Jose E. Marchesi

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

(use-modules (gdb))

;; Pretty-printer for pvm_val objects.

(define (pp-pvm-val value)
  (let ((pvm-tag (value-logand value #b111)))
    (case (value->integer pvm-tag)
      ((#x0) ;; PVM_VAL_TAG_INT
       (let* ((int-size (value-add (value-cast (value-logand (value-rsh value 3) #x1f)
                                               (lookup-type "int32_t"))
                                   1))
              (int-val (value-rsh (value-lsh (value-cast
                                              (value-rsh value 32)
                                              (lookup-type "int32_t"))
                                             (value-sub 32 int-size))
                                  (value-sub 32 int-size))))
         (format #f "(pvm:int<~a>) ~a" int-size int-val)))
      ((#x1) ;; PVM_VAL_TAG_UINT
       (let* ((uint-size (value-add (value-cast (value-logand (value-rsh value 3) #x1f)
                                                (lookup-type "int32_t"))
                                    1))
              (uint-val (value-logand (value-cast
                                       (value-rsh value 32)
                                       (lookup-type "uint32_t"))
                                      (value-cast
                                       (value-lognot
                                        (value-lsh
                                         (value-lsh
                                          (value-lognot
                                           (value-cast (make-value 0)
                                                       (lookup-type "uint64_t")))
                                          (value-sub uint-size 1))
                                         (make-value 1)))
                                       (lookup-type "uint32_t")))))
         (format #f "(pvm:uint<~a>) ~a" uint-size uint-val)))
      ((#x2) ;; PVM_VAL_TAG_LONG
       (let* ((long-ulong-val (value-subscript
                               (value-cast
                                (value-logand (value-cast value (lookup-type "uintptr_t"))
                                              (value-lognot #x7))
                                (type-pointer (lookup-type "int64_t")))
                               0))
              (long-size (value-add
                          (value-subscript
                           (value-cast
                            (value-logand (value-cast value (lookup-type "uintptr_t"))
                                          (value-lognot #x7))
                            (type-pointer (lookup-type "int64_t")))
                           1)
                          1))
              (long-val (value-rsh (value-lsh
                                    long-ulong-val
                                    (value-sub 64 long-size))
                                   (value-sub 64 long-size))))
         (format #f "(pvm:long<~a>) ~a" long-size long-val)))
      ((#x3) ;; PVM_VAL_TAG_ULONG
       (let* ((long-ulong-val (value-subscript
                               (value-cast
                                (value-logand (value-cast value (lookup-type "uintptr_t"))
                                              (value-lognot #x7))
                                (type-pointer (lookup-type "int64_t")))
                               0))
              (ulong-size (value-add
                           (value-subscript
                            (value-cast
                             (value-logand (value-cast value (lookup-type "uintptr_t"))
                                           (value-lognot #x7))
                             (type-pointer (lookup-type "int64_t")))
                            1)
                           1))
              (ulong-val (value-logand
                          long-ulong-val
                          (value-cast (value-lognot
                                       (value-lsh
                                        (value-lsh
                                         (value-lognot
                                          (value-cast (make-value 0)
                                                      (lookup-type "unsigned long long")))
                                         (value-sub ulong-size 1))
                                        (make-value 1)))
                                      (lookup-type "uint64_t")))))
         (format #f "(pvm:ulong<~a>) ~a" ulong-size ulong-val)))
      ((#x6) ;; PVM_VAL_TAG_BOX
       (let* ((pvm-box-ptr (value-cast (value-logand
                                        (value-cast value
                                                    (lookup-type "uintptr_t"))
                                        (value-lognot #x7))
                                       (lookup-type "pvm_val_box")))
              (pvm-box (value-dereference pvm-box-ptr))
              (pvm-box-tag (value-field pvm-box "tag")))
         (case (value->integer pvm-box-tag)
           ((#x8) ;; PVM_VAL_TAG_STR
            "STRING")
           ((#x9) ;; PVM_VAL_TAG_OFF
            (let ((offset (value-dereference (value-field (value-field pvm-box "v")
                                                          "offset"))))
              (format #f "(pvm:offset) [~a ~a]"
                      (pp-pvm-val (value-field offset "magnitude"))
                      (pp-pvm-val (value-field offset "unit")))))
           ((#xa) ;; PVM_VAL_TAG_ARR
            (let* ((array (value-dereference (value-field (value-field pvm-box "v")
                                                          "array"))))
              array))
           ((#xb) ;; PVM_VAL_TAG_SCT
            (let* ((struct (value-dereference (value-field (value-field pvm-box "v")
                                                           "sct"))))
              sct))
           ((#xc) ;; PVM_VAL_TAG_TYP
            "TYPE")
           ((#xd) ;; PVM_VAL_TAG_MAP
            "MAP")
           ((#xe) ;; PVM_VAL_TAG_CLS
            "CLS")
           (else
            "Unknown PVM_VAL_BOX tag"))))
      ((#x7) ;; PVM_NULL
       "PVM_NULL")
      (else
       "Unknown PVM_VAL tag"))))

(define (make-poke-pvm-val-printer value)
  "Print a pvm_val object"
  (make-pretty-printer-worker
   #f
   (lambda (printer)
     (pp-pvm-val value))
   #f))

(define (poke-lookup-function pretty-printer value)
  (let ((tname (type-name (value-type value))))
    (and tname
         (equal? tname "pvm_val")
         (make-poke-pvm-val-printer value))))

(set-pretty-printers!
 (cons
  (make-pretty-printer "pvm_val" poke-lookup-function)
  (pretty-printers)))
