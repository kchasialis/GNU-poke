;;; -*- mode: poke-ras -*-
;;; pkl-gen.pks - Assembly routines for the codegen
;;;

;;; Copyright (C) 2019, 2020 Jose E. Marchesi

;;; This program is free software: you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation, either version 3 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY ; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with this program.  If not, see <http: //www.gnu.org/licenses/>.

;;; RAS_MACRO_OP_UNMAP
;;; ( VAL -- VAL )
;;;
;;; Turn the value on the stack into a non-mapped value, if the value
;;; is mapped.  If the value is not mapped, this is a NOP.

        .macro op_unmap
        push null
        mseto
        push null
        msetm
        push null
        msetw
        push null
        msetios
        .end

;;; RAS_FUNCTION_ARRAY_MAPPER
;;; ( IOS BOFF EBOUND SBOUND -- ARR )
;;;
;;; Assemble a function that maps an array value at the given
;;; bit-offset BOFF in the IO space IOS, with mapping attributes
;;; EBOUND and SBOUND.
;;;
;;; If both EBOUND and SBOUND are null, then perform an unbounded map,
;;; i.e. read array elements from IO until EOF.
;;;
;;; Otherwise, if EBOUND is not null, then perform a map bounded by the
;;; given number of elements.  If EOF is encountered before the given
;;; amount of elements are read, then raise PVM_E_MAP_BOUNDS.
;;;
;;; Otherwise, if SBOUND is not null, then perform a map bounded by the
;;; given size (a bit-offset), i.e. read array elements from IO until
;;; the total size of the array is exactly SBOUND.  If SBOUND is exceeded,
;;; then raise PVM_E_MAP_BOUNDS.
;;;
;;; Only one of EBOUND or SBOUND simultanously are supported.
;;; Note that OFF should be of type offset<uint<64>,*>.
;;;
;;; The C environment required is:
;;;
;;; `array_type' is a pkl_ast_node with the array type being mapped.

        .function array_mapper
        prolog
        pushf 6
        regvar $sbound           ; Argument
        regvar $ebound           ; Argument
        regvar $boff             ; Argument
        regvar $ios              ; Argument
        ;; Initialize the bit-offset of the elements in a local.
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        regvar $eboff           ; BOFF
        ;; Initialize the element index to 0UL, and put it
        ;; in a local.
        push ulong<64>0         ; BOFF 0UL
        regvar $eidx            ; BOFF
        ;; Build the type of the new mapped array.  Note that we use
        ;; the bounds passed to the mapper instead of just subpassing
        ;; in array_type.  This is because this mapper should work for
        ;; both bounded and unbounded array types.  Also, this avoids
        ;; evaluating the boundary expression in the array type
        ;; twice.
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; OFF ETYPE
        pushvar $ebound         ; OFF ETYPE EBOUND
        bnn .atype_bound_done
        drop                    ; OFF ETYPE
        pushvar $sbound         ; OFF ETYPE (SBOUND|NULL)
.atype_bound_done:
        mktya                   ; OFF ATYPE
        .while
        ;; If there is an EBOUND, check it.
        ;; Else, if there is a SBOUND, check it.
        ;; Else, iterate (unbounded).
        pushvar $ebound         ; OFF ATYPE NELEM
        bn .loop_on_sbound
        pushvar $eidx           ; OFF ATYPE NELEM I
        gtlu                    ; OFF ATYPE NELEM I (NELEM>I)
        nip2                    ; OFF ATYPE (NELEM>I)
        ba .end_loop_on
.loop_on_sbound:
        drop                    ; OFF ATYPE
        pushvar $sbound         ; OFF ATYPE SBOUND
        bn .loop_unbounded
        pushvar $boff           ; OFF ATYPE SBOUND BOFF
        addlu                   ; OFF ATYPE SBOUND BOFF (SBOUND+BOFF)
        nip2                    ; OFF ATYPE (SBOUND+BOFF)
        pushvar $eboff          ; OFF ATYPE (SBOUND+BOFF) EBOFF
        gtlu                    ; OFF ATYPE (SBOUND+BOFF) EBOFF ((SBOUND+BOFF)>EBOFF)
        nip2                    ; OFF ATYPE ((SBOUND+BOFF)>EBOFF)
        ba .end_loop_on
.loop_unbounded:
        drop                    ; OFF ATYPE
        push int<32>1           ; OFF ATYPE 1
.end_loop_on:
        .loop
                                ; OFF ATYPE
        ;; Mount the Ith element triplet: [EBOFF EIDX EVAL]
        pushvar $eboff          ; ... EBOFF
        dup                     ; ... EBOFF EBOFF
        push PVM_E_EOF
        pushe .eof
        push PVM_E_CONSTRAINT
        pushe .constraint_error
        pushvar $ios            ; ... EBOFF EBOFF IOS
        swap                    ; ... EBOFF IOS EBOFF
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        pope
        pope
        ;; Update the current offset with the size of the value just
        ;; peeked.
        siz                     ; ... EBOFF EVAL ESIZ
        quake                   ; ... EVAL EBOFF ESIZ
        addlu                   ; ... EVAL EBOFF ESIZ (EBOFF+ESIZ)
        popvar $eboff           ; ... EVAL EBOFF ESIZ
        drop                    ; ... EVAL EBOFF
        pushvar $eidx           ; ... EVAL EBOFF EIDX
        rot                     ; ... EBOFF EIDX EVAL
        ;; Increase the current index and process the next element.
        pushvar $eidx           ; ... EBOFF EIDX EVAL EIDX
        push ulong<64>1         ; ... EBOFF EIDX EVAL EIDX 1UL
        addlu                   ; ... EBOFF EIDX EVAL EDIX 1UL (EIDX+1UL)
        nip2                    ; ... EBOFF EIDX EVAL (EIDX+1UL)
        popvar $eidx            ; ... EBOFF EIDX EVAL
        .endloop
        push null
        ba .mountarray
.constraint_error:
        ;; Remove the partial element from the stack.
                                ; ... EOFF EOFF EXCEPTION
        drop
        drop
        drop
        ;; If the array is bounded, raise E_CONSTRAINT
        pushvar $ebound         ; ... EBOUND
        nn                      ; ... EBOUND (EBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL)
        pushvar $sbound         ; ... (EBOUND!=NULL) SBOUND
        nn                      ; ... (EBOUND!=NULL) SBOUND (SBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL) (SBOUND!=NULL)
        or                      ; ... (EBOUND!=NULL) (SBOUND!=NULL) ARRAYBOUNDED
        nip2                    ; ... ARRAYBOUNDED
        bzi .mountarray
        push PVM_E_CONSTRAINT
        raise
.eof:
        ;; Remove the partial EOFF null element from the stack.
        drop
                                ; ... EOFF null
        drop                    ; ... EOFF
        drop                    ; ...
        ;; If the array is bounded, raise E_EOF
        pushvar $ebound         ; ... EBOUND
        nn                      ; ... EBOUND (EBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL)
        pushvar $sbound         ; ... (EBOUND!=NULL) SBOUND
        nn                      ; ... (EBOUND!=NULL) SBOUND (SBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL) (SBOUND!=NULL)
        or                      ; ... (EBOUND!=NULL) (SBOUND!=NULL) ARRAYBOUNDED
        nip2                    ; ... ARRAYBOUNDED
        bzi .mountarray
        push PVM_E_EOF
        raise
.mountarray:
        drop                   ; BOFF ATYPE [EBOFF EIDX EVAL]...
        pushvar $eidx          ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM
        dup                    ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM NINITIALIZER
        mka                    ; ARRAY
        ;; Check that the resulting array satisfies the mapping's
        ;; bounds (number of elements and total size.)
        pushvar $ebound        ; ARRAY EBOUND
        bnn .check_ebound
        drop                   ; ARRAY
        pushvar $sbound        ; ARRAY SBOUND
        bnn .check_sbound
        drop
        ba .bounds_ok
.check_ebound:
        swap                   ; EBOUND ARRAY
        sel                    ; EBOUND ARRAY NELEM
        rot                    ; ARRAY NELEM EBOUND
        sublu                  ; ARRAY NELEM EBOUND (NELEM-EBOUND)
        bnzlu .bounds_fail
        drop                   ; ARRAY NELEM EBOUND
        drop                   ; ARRAY NELEM
        drop                   ; ARRAY
        ba .bounds_ok
.check_sbound:
        swap                   ; SBOUND ARRAY
        siz                    ; SBOUND ARRAY SIZ
        rot                    ; ARRAY SIZ SBOUND
        sublu                  ; ARRAY SIZ SBOUND (SIZ-SBOUND)
        bnzlu .bounds_fail
        drop                   ; ARRAY (OFFU*OFFM) SBOUND
        drop                   ; ARRAY (OFFU*OFFM)
        drop                   ; ARRAY
.bounds_ok:
        ;; Set the map bound attributes in the new object.
        pushvar $sbound       ; ARRAY SBOUND
        msetsiz               ; ARRAY
        pushvar $ebound       ; ARRAY EBOUND
        msetsel               ; ARRAY
        ;; Set the other map attributes.
        pushvar $ios          ; ARRAY IOS
        msetios               ; ARRAY
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_FUNCTION_ARRAY_VALMAPPER
;;; ( VAL NVAL BOFF -- ARR )
;;;
;;; Assemble a function that "valmaps" a given NVAL at the given
;;; bit-offset BOFF, using the data of NVAL, and the mapping
;;; attributes of VAL.
;;;
;;; This function can raise PVM_E_MAP_BOUNDS if the characteristics of
;;; NVAL violate the bounds of the map.
;;;
;;; Note that OFF should be of type offset<uint<64>,*>.

        .function array_valmapper
        prolog
        pushf 8
        regvar $boff            ; Argument
        regvar $nval            ; Argument
        regvar $val             ; Argument
        ;; Determine VAL's bounds and set them in locals to be used
        ;; later.
        pushvar $val            ; VAL
        mgetsel                 ; VAL EBOUND
        regvar $ebound          ; VAL
        mgetsiz                 ; VAL SBOUND
        regvar $sbound          ; VAL
        drop                    ; _
        ;; Initialize the bit-offset of the elements in a local.
        pushvar $boff           ; BOFF
        dup
        regvar $eboff           ; BOFF
        ;; Initialize the element index to 0UL, and put it
        ;; in a local.
        push ulong<64>0         ; BOFF 0UL
        regvar $eidx            ; BOFF
        ;; Get the number of elements in NVAL, and put it in a local.
        pushvar $nval           ; BOFF NVAL
        sel                     ; BOFF NVAL NELEM
        nip                     ; BOFF NELEM
        regvar $nelem           ; BOFF
        ;; Check that NVAL satisfies EBOUND if this bound is specified
        ;; i.e. the number of elements stored in the array matches the
        ;; bound.
        pushvar $ebound         ; BOFF EBOUND
        bnn .check_ebound
        drop                    ; BOFF
        ba .ebound_ok
.check_ebound:
        pushvar $nelem          ; BOFF EBOUND NELEM
        sublu                   ; BOFF EBOUND NELEM (EBOUND-NELEM)
        bnzlu .bounds_fail
        drop                    ; BOFF EBOUND NELEM
        drop                    ; BOFF EBOUND
        drop                    ; BOFF
.ebound_ok:
        ;; Build the type of the new mapped array.  Note that we use
        ;; the bounds extracted above instead of just subpassing in
        ;; array_type.  This is because this function should work for
        ;; both bounded and unbounded array types.  Also, this avoids
        ;; evaluating the boundary expression in the array type
        ;; twice.
        .c PKL_GEN_PAYLOAD->in_valmapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_valmapper = 1;
                                ; BOFF ETYPE
        pushvar $ebound         ; BOFF ETYPE EBOUND
        bnn .atype_bound_done
        drop                    ; BOFF ETYPE
        pushvar $sbound         ; BOFF ETYPE (SBOUND|NULL)
.atype_bound_done:
        mktya                   ; BOFF ATYPE
        .while
        pushvar $eidx           ; BOFF ATYPE I
        pushvar $nelem          ; BOFF ATYPE I NELEM
        ltlu                    ; BOFF ATYPE I NELEM (NELEM<I)
        nip2                    ; BOFF ATYPE (NELEM<I)
        .loop
                                ; BOFF ATYPE

        ;; Mount the Ith element triplet: [EBOFF EIDX EVAL]
        pushvar $eboff          ; ... EBOFF
        dup                     ; ... EBOFF EBOFF
        pushvar $nval           ; ... EBOFF EBOFF NVAL
        pushvar $eidx           ; ... EBOFF EBOFF NVAL IDX
        aref                    ; ... EBOFF EBOFF NVAL IDX ENVAL
        nip2                    ; ... EBOFF EBOFF ENVAL
        swap                    ; ... EBOFF ENVAL EBOFF
        pushvar $val            ; ... EBOFF ENVAL EBOFF VAL
        pushvar $eidx           ; ... EBOFF ENVAL EBOFF VAL EIDX
        aref                    ; ... EBOFF ENVAL EBOFF VAL EIDX OVAL
        nip2                    ; ... EBOFF ENVAL EBOFF OVAL
        nrot                    ; ... EBOFF OVAL ENVAL EBOFF
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
                                ; ... EBOFF EVAL
        ;; Update the current offset with the size of the value just
        ;; peeked.
        siz                     ; ... EBOFF EVAL ESIZ
        quake                   ; ... EVAL EBOFF ESIZ
        addlu                   ; ... EVAL EBOFF ESIZ (EBOFF+ESIZ)
        popvar $eboff           ; ... EVAL EBOFF ESIZ
        drop                    ; ... EVAL EBOFF
        pushvar $eidx           ; ... EVAL EBOFF EIDX
        rot                     ; ... EBOFF EIDX EVAL
        ;; Increase the current index and process the next element.
        pushvar $eidx           ; ... EBOFF EIDX EVAL EIDX
        push ulong<64>1         ; ... EBOFF EIDX EVAL EIDX 1UL
        addlu                   ; ... EBOFF EIDX EVAL EDIX 1UL (EIDX+1UL)
        nip2                    ; ... EBOFF EIDX EVAL (EIDX+1UL)
        popvar $eidx            ; ... EBOFF EIDX EVAL
        .endloop
        pushvar $eidx           ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM
        dup                     ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM NINITIALIZER
        mka                     ; ARRAY
        ;; Check that the resulting array satisfies the mapping's
        ;; total size bound.
        pushvar $sbound         ; ARRAY SBOUND
        bnn .check_sbound
        drop
        ba .sbound_ok
.check_sbound:
        swap                    ; SBOUND ARRAY
        siz                     ; SBOUND ARRAY SIZ
        rot                     ; ARRAY SIZ SBOUND
        sublu                   ; ARRAY SIZ SBOUND (SIZ-SBOUND)
        bnzlu .bounds_fail
        drop                    ; ARRAY (OFFU*OFFM) SBOUND
        drop                    ; ARRAY (OFFU*OFFM)
        drop                    ; ARRAY
.sbound_ok:
        ;; Set the map bound attributes in the new object.
        pushvar $sbound         ; ARRAY SBOUND
        msetsiz                 ; ARRAY
        pushvar $ebound         ; ARRAY EBOUND
        msetsel                 ; ARRAY
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_FUNCTION_ARRAY_WRITER
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped array value.
;;;
;;; Note that it is important for the elements of the array to be
;;; poked in order.

        .function array_writer
        prolog
        pushf 3
        mgetios                 ; ARRAY IOS
        regvar $ios             ; ARRAY
        regvar $value           ; _
        push ulong<64>0         ; 0UL
        regvar $idx             ; _
     .while
        pushvar $idx            ; I
        pushvar $value          ; I ARRAY
        sel                     ; I ARRAY NELEM
        nip                     ; I NELEM
        ltlu                    ; I NELEM (NELEM<I)
        nip2                    ; (NELEM<I)
     .loop
                                ; _
        ;; Poke this array element
        pushvar $value          ; ARRAY
        pushvar $idx            ; ARRAY I
        aref                    ; ARRAY I VAL
        nrot                    ; VAL ARRAY I
        arefo                   ; VAL ARRAY I EBOFF
        nip2                    ; VAL EBOFF
        swap                    ; EBOFF VAL
        pushvar $ios            ; EBOFF VAL IOS
        nrot                    ; IOS EOFF VAL
        .c PKL_GEN_PAYLOAD->in_writer = 1;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_writer = 0;
                                ; _
        ;; Increase the current index and process the next
        ;; element.
        pushvar $idx            ; EIDX
        push ulong<64>1         ; EIDX 1UL
        addlu                   ; EDIX 1UL (EIDX+1UL)
        nip2                    ; (EIDX+1UL)
        popvar $idx             ; _
     .endloop
        popf 1
        push null
        return
        .end                    ; array_writer

;;; RAS_FUNCTION_ARRAY_BOUNDER
;;; ( _ -- BOUND )
;;;
;;; Assemble a function that returns the boundary of an array type.
;;; If the array type is not bounded by either number of elements nor size
;;; then PVM_NULL is returned.
;;;
;;; Note how this function doesn't introduce any lexical level.  This
;;; is important, so keep it this way!
;;;
;;; The C environment required is:
;;;
;;; `array_type' is a pkl_ast_node with the type of ARR.

        .function array_bounder
        prolog
        .c if (PKL_AST_TYPE_A_BOUND (array_type))
        .c {
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 0;
        .c   PKL_PASS_SUBPASS (PKL_AST_TYPE_A_BOUND (array_type)) ;
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 1;
        .c }
        .c else
             push null
        return
        .end

;;; RAS_FUNCTION_ARRAY_CONSTRUCTOR
;;; ( EBOUND SBOUND -- ARR )
;;;
;;; Assemble a function that constructs an array value of a given
;;; type, with default values.
;;;
;;; EBOUND and SBOUND determine the bounding of the array.  If both
;;; are null, then the array is unbounded.  Otherwise, only one of
;;; EBOUND and SBOUND can be provided.
;;;
;;; Empty arrays are always constructed for unbounded arrays.
;;;
;;; The C environment required is:
;;;
;;; `array_type' is a pkl_ast_node with the array type being constructed.

        .function array_constructor
        prolog
        pushf 4                 ; EBOUND SBOUND
        ;; If both bounds are null, then ebound is 0.
        bn .sbound_nil
        ba .bounds_ready
.sbound_nil:
        swap                    ; SBOUND EBOUND
        bn .ebound_nil
        swap                    ; EBOUND SBOUND
        ba .bounds_ready
.ebound_nil:
        drop                    ; null
        push ulong<64>0         ; null 0UL
        swap                    ; 0UL null
.bounds_ready:
        regvar $sbound          ; EBOUND
        regvar $ebound          ; _
        ;; Initialize the element index and the bit cound, and put them
        ;; in locals.
        push ulong<64>0         ; 0UL
        dup                     ; 0UL 0UL
        regvar $eidx            ; BOFF
        regvar $eboff           ; _
        ;; The offset of the constructed array is null, since it is
        ;; not mapped.
        push null               ; null
        ;; Build the type of the constructed array.
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
                                ; null ATYPE
        ;; Ok, loop to add elements to the constructed array.
     .while
        ;; If there is an EBOUND, check it.
        ;; Else, check the SBOUND.
        pushvar $ebound         ; NELEM
        bn .loop_on_sbound
        pushvar $eidx           ; NELEM I
        gtlu                    ; NELEM I (NELEM>I)
        nip2
        ba .end_loop_on
.loop_on_sbound:
        drop
        pushvar $sbound         ; SBOUND
        pushvar $eboff          ; SBOUND EBOFF
        gtlu                    ; SBOUND EBOFF (SBOUNDMAG>EBOFF)
        nip2
.end_loop_on:
     .loop
        ;; Mount the Ith element triplet: [EBOFF EIDX EVAL]
        pushvar $eboff          ; ... EBOFF
        pushvar $eidx           ; ... EBOFF EIDX
        push null               ; ... EBOFF null
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
                                ; ... EBOFF EIDX EVAL
        ;; Update the bit offset.
        siz                     ; ... EBOFF EIDX EVAL ESIZ
        pushvar $eboff          ; ... EBOFF EIDX EVAL ESIZ EBOFF
        addlu
        nip2                    ; ... EBOFF EIDX EVAL NEBOFF
        popvar $eboff           ; ... EBOFF EIDX EVAL
        ;; Update the index.
        over                    ; ... EBOFF EIDX EVAL EIDX
        push ulong<64>1         ; ... EBOFF EIDX EVAL EIDX 1UL
        addlu
        nip2                    ; ... EBOFF EIDX EVAL (EIDX+1UL)
        popvar $eidx
     .endloop
        pushvar $eidx           ; null ATYPE [EBOFF EIDX EVAL]... NELEM
        dup                     ; null ATYPE [EBOFF EIDX EVAL]... NELEM NINITIALIZER
        mka                     ; ARRAY
        ;; Check that the resulting array satisfies the size bound.
        pushvar $sbound         ; ARRAY SBOUND
        bn .bounds_ok
        swap                    ; SBOUND ARRAY
        siz                     ; SBOUND ARRAY SIZ
        rot                     ; ARRAY SIZ SBOUND
        sublu                   ; ARRAY SIZ SBOUND (SIZ-SBOUND)
        bnzlu .bounds_fail
        drop                   ; ARRAY (OFFU*OFFM) SBOUND
        drop                   ; ARRAY (OFFU*OFFM)
.bounds_ok:
        drop                   ; ARRAY
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_LABEL
;;; ( BOFF SBOFF - BOFF )
;;;
;;; Given a struct type element, it's offset and the offset of the struct
;;; on the stack, increase the bit-offset by the element's label, in
;;; case it exists.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.

        .macro handle_struct_field_label
   .c if (PKL_AST_STRUCT_TYPE_FIELD_LABEL (field) == NULL)
        drop                    ; BOFF
   .c else
   .c {
        nip                     ; SBOFF
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_LABEL (field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; SBOFF LOFF
        ogetm                   ; SBOFF LOFF LOFFM
        swap                    ; SBOFF LOFFM LOFF
        ogetu                   ; SBOFF LOFFM LOFF LOFFU
        nip                     ; SBOFF LOFFM LOFFU
        mullu
        nip2                    ; SBOFF (LOFFM*LOFFU)
        addlu
        nip2                    ; (SBOFF+LOFFM*LOFFU)
   .c }
        .end

;;; RAS_MACRO_CHECK_STRUCT_FIELD_CONSTRAINT
;;; ( -- )
;;;
;;; Evaluate the given struct field's constraint, raising an
;;; exception if not satisfied.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.

        .macro check_struct_field_constraint
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field) != NULL)
   .c {
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
        bnzi .constraint_ok
        drop
        push PVM_E_CONSTRAINT
        raise
.constraint_ok:
        drop
   .c }
        .end

;;; RAS_MACRO_STRUCT_FIELD_MAPPER
;;; ( IOS BOFF SBOFF -- BOFF STR VAL NBOFF )
;;;
;;; Map a struct field from the current IOS.
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;; by this macro.  It is typically ulong<64>0 or ulong<64>1.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro struct_field_mapper
        ;; Increase OFF by the label, if the field has one.
        .e handle_struct_field_label    ; IOS BOFF
        dup                             ; IOS BOFF BOFF
        nrot                            ; BOFF IOS BOFF
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (field));
        .c PKL_GEN_PAYLOAD->endian = endian; }
                                        ; BOFF VAL
        dup                             ; BOFF VAL VAL
        regvar $val                     ; BOFF VAL
        .c vars_registered++;
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (field));
                                        ; BOFF VAL STR
        swap                            ; BOFF STR VAL
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (field);
   .c if (optcond)
        .c {
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (optcond);
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
        bnzi .optcond_ok
        drop                    ; BOFF STR VAL
        drop                    ; BOFF STR
        drop                    ; BOFF
        push null               ; BOFF null
        over                    ; BOFF null BOFF
        over                    ; BOFF null BOFF null
        dup                     ; BOFF null BOFF null null
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_POPVAR,
        .c               0 /* back */, 3 + vars_registered - 1 /* over */);
        swap                    ; BOFF null null BOFF
        ba .omitted_field
   .c }
   .c else
   .c {
        push null               ; BOFF STR VAL null
   .c }
.optcond_ok:
        drop                    ; BOFF STR VAL
        ;; Evaluate the field's constraint and raise
        ;; an exception if not satisfied.
        .e check_struct_field_constraint
        ;; Calculate the offset marking the end of the field, which is
        ;; the field's offset plus it's size.
        quake                  ; STR BOFF VAL
        siz                    ; STR BOFF VAL SIZ
        quake                  ; STR VAL BOFF SIZ
        addlu
        nip                    ; STR VAL BOFF (BOFF+SIZ)
        tor                    ; STR VAL BOFF
        nrot                   ; BOFF STR VAL
        fromr                  ; BOFF STR VAL NBOFF
.omitted_field:
        .end

;;; RAS_FUNCTION_STRUCT_MAPPER
;;; ( IOS BOFF EBOUND SBOUND -- SCT )
;;;
;;; Assemble a function that maps a struct value at the given offset
;;; OFF.
;;;
;;; Both EBOUND and SBOUND are always null, and not used, i.e. struct maps
;;; are not bounded by either number of fields or size.
;;;
;;; BOFF should be of type uint<64>.
;;;
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        ;; NOTE: please be careful when altering the lexical structure of
        ;; this code (and of the code in expanded macros). Every local
        ;; added should be also reflected in the compile-time environment
        ;; in pkl-tab.y, or horrible things _will_ happen.  So if you
        ;; add/remove locals here, adjust accordingly in
        ;; pkl-tab.y:struct_type_specifier.  Thank you very mucho!

        .function struct_mapper
        prolog
        pushf 3
        drop                    ; sbound
        drop                    ; ebound
        regvar $boff
        regvar $ios
        push ulong<64>0
        regvar $nfield
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        ;; Iterate over the elements of the struct type.
 .c size_t vars_registered = 0;
 .c for (field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c   if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_mapper = 0;
 .c     PKL_PASS_SUBPASS (field);
 .c     PKL_GEN_PAYLOAD->in_mapper = 1;
 .c
 .c     continue;
 .c   }
        .label .alternative_failed
        .label .eof_in_alternative
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        push PVM_E_EOF
        pushe .eof_in_alternative
        push PVM_E_CONSTRAINT
        pushe .alternative_failed
 .c   }
        pushvar $ios             ; ...[EBOFF ENAME EVAL] NEBOFF IOS
        swap                     ; ...[EBOFF ENAME EVAL] IOS NEBOFF
        pushvar $boff            ; ...[EBOFF ENAME EVAL] IOS NEBOFF OFF
        .e struct_field_mapper   ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        pope
        pope
 .c   }
        ;; Increase the number of fields.
        pushvar $nfield         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD
        push ulong<64>1         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD 1UL
        addlu
        nip2                    ; ...[EBOFF ENAME EVAL] NEBOFF (NFIELD+1UL)
        popvar $nfield          ; ...[EBOFF ENAME EVAL] NEBOFF
        ;; If the struct is pinned, replace NEBOFF with BOFF
 .c   if (PKL_AST_TYPE_S_PINNED (type_struct))
 .c   {
        drop
        pushvar $boff           ; ...[EBOFF ENAME EVAL] BOFF
 .c   }
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        ;; Union field successfully mapped.  We are done.
        ba .union_fields_done
.eof_in_alternative:
        ;; If we got EOF in an union alternative, and this is the last
        ;; alternative in the union, re-raise it.  Otherwise just
        ;; try the next alternative.
     .c if (PKL_AST_CHAIN (field) == NULL)
     .c {
        raise
     .c }
.alternative_failed:
        ;; Drop the exception and try next alternative.
        drop                    ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c }
 .c if (PKL_AST_TYPE_S_UNION (type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        drop                    ; ...[EBOFF ENAME EVAL]
        ;; Ok, at this point all the struct field triplets are
        ;; in the stack.
        ;; Iterate over the methods of the struct type.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c   if (PKL_AST_CODE (field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (field)))
 .c   {
 .c     if (PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 3 +
        ;; element order.  This 3 should be updated if the lexical
        ;; structure of this function changes.
        ;;
        ;; XXX note that here we really want to duplicate the
        ;; environment of the closure, to avoid all the methods
        ;; to share an environment.  PVM instruction for that?
        ;; Sounds good.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH,
 .c                   pvm_make_string (PKL_AST_IDENTIFIER_POINTER (PKL_AST_DECL_NAME (field))));
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 3 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (nmethod, 64));
 .c }
        ;;  Push the number of fields
        pushvar $nfield         ; BOFF [EBOFF STR VAL]... NFIELD
        ;; Finally, push the struct type and call mksct.
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (type_struct);
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; BOFF [EBOFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        ;; Install the attributes of the mapped object.
        pushvar $ios            ; SCT IOS
        msetios                 ; SCT
        popf 1
        return
        .end

;;; RAS_FUNCTION_STRUCT_COMPARATOR
;;; ( SCT SCT -- INT )
;;;
;;; Assemble a function that, given two structs of a given type,
;;; return 1 if the two structs are equal, 0 otherwise.
;;;
;;; The C environment erquired is:
;;;
;;; `type_struct' is a pkl_ast_node with the types of the structs
;;;  being compared.

        ;; NOTE: this function should have the same lexical structure
        ;; than struct_mapper above.  If you add more local variables,
        ;; please adjust struct_comparator accordingly, and also follow
        ;; the instructions on the NOTE there.

        .function struct_comparator
        prolog
.c { uint64_t i; pkl_ast_node field;
 .c  for (i = 0, field = PKL_AST_TYPE_S_ELEMS (type_struct);
 .c       field;
 .c       field = PKL_AST_CHAIN (field), ++i)
 .c  {
 .c     pkl_ast_node field_type
 .c       = PKL_AST_STRUCT_TYPE_FIELD_TYPE (field) ;
 .c
 .c     if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Compare the fields of both structs.
        tor                     ; SCT1 [SCT2]
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (i, 64));
                                ; SCT1 I [SCT2]
        srefi                   ; SCT1 I VAL1 [SCT2]
        swap                    ; SCT1 VAL1 I [SCT2]
        fromr                   ; SCT1 VAL1 I SCT2
        swap                    ; SCT1 VAL1 SCT2 I
        srefi                   ; SCT1 VAL1 SCT2 I VAL2
        nip                     ; SCT1 VAL1 SCT2 VAL2
        quake                   ; SCT1 SCT2 VAL1 VAL2
 .c if (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (field))
 .c {
        ;; If the field is optional, both VAL1 and VAL2 can be null.
        ;; In that case the fields are considered equal only if they are
        ;; both null.  We try to avoid conditional jumps here:
        ;;
        ;; val1n val2n  equal?  val1n+val2n  val1n+val2n-1
        ;;   0     0    maybe        0
        ;;   0     1    no           1              0 -\
        ;;   1     0    no           1              0 --> desired truth
        ;;   1     1    yes          2              1 -/  value
        .label .do_compare
        nnn                     ; SCT1 SCT2 VAL1 VAL2 VAL2N
        rot                     ; SCT1 SCT2 VAL2 VAL2N VAL1
        nnn                     ; SCT1 SCT2 VAL2 VAL2N VAL1 VAL1N
        rot                     ; SCT1 SCT2 VAL2 VAL1 VAL1N VAL2N
        addi
        nip2                    ; SCT1 SCT2 VAL2 VAL1 (VAL1N+VAL2N)
        quake                   ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N)
        bzi .do_compare
        push int<32>1           ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N) 1
        subi
        nip2                    ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N-1)
        nip2                    ; SCT1 SCT2 (VAL1N+VAL2N-1)
        ba .done
.do_compare:
        drop                    ; SCT1 SCT2 VAL1 VAL2
 .c }
        ;; Note that we cannot use EQ if the field is a struct itself,
        ;; because EQ uses comparators!  So we subpass instead.  :)
 .c     if (PKL_AST_TYPE_CODE (field_type) == PKL_TYPE_STRUCT)
 .c       PKL_PASS_SUBPASS (field_type);
 .c     else
 .c     {
 .c       pkl_asm_insn (RAS_ASM, PKL_INSN_EQ,
 .c                     PKL_AST_STRUCT_TYPE_FIELD_TYPE (field));
 .c     }
        nip2                    ; SCT1 SCT2 (VAL1==VAL2)
        bzi .done
        drop                    ; SCT1 SCT2
 .c  }
.c }
        ;; The structs are equal
        push int<32>1           ; SCT1 SCT2 1
.done:
        nip2                    ; INT
        return
        .end

;;; RAS_FUNCTION_STRUCT_CONSTRUCTOR
;;; ( SCT -- SCT )
;;;
;;; Assemble a function that constructs a struct value of a given type
;;; from another struct value.
;;;
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        ;; NOTE: this function should have the same lexical structure
        ;; than struct_mapper above.  If you add more local variables,
        ;; please adjust struct_mapper accordingly, and also follow the
        ;; instructions on the NOTE there.

        .function struct_constructor
        prolog
        pushf 3
        regvar $sct             ; SCT
        ;; Initialize $nfield to 0UL
        push ulong<64>0
        regvar $nfield
        ;; Initialize $boff to 0UL#b.
        push ulong<64>0
        regvar $boff
        ;; The struct is not mapped, so its offset is null.
        push null               ; null
        ;; Iterate over the fields of the struct type.
 .c size_t vars_registered = 0;
 .c for (field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
        .label .alternative_failed
        .label .constraint_ok
        .label .optcond_ok
        .label .omitted_field
        .label .got_value
 .c   pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (field);
 .c   pkl_ast_node field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (field);
 .c   pkl_ast_node field_initializer = PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (field);
 .c   if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_constructor = 0;
 .c     PKL_PASS_SUBPASS (field);
 .c     PKL_GEN_PAYLOAD->in_constructor = 1;
 .c
 .c     continue;
 .c   }
        pushvar $sct           ; ... [EBOFF ENAME EVAL] SCT
 .c   if (field_name)
 .c   {
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH,
 .c                   pvm_make_string (PKL_AST_IDENTIFIER_POINTER (field_name)));
                               ; ... SCT ENAME
        ;; Get the value of the field in $sct.
        srefnt                 ; ... SCT ENAME EVAL
 .c   }
 .c   else
 .c   {
        push null               ; ... SCT ENAME
        push null               ; ... SCT ENAME EVAL
 .c   }
        ;; If the value is not-null, use it.  Otherwise, use the value
        ;; obtained by subpassing in the value's type, or the field's
        ;; initializer.
        bnn .got_value         ; ... SCT ENAME null
        .c if (field_initializer)
        .c {
        drop
        .c   PKL_GEN_PAYLOAD->in_constructor = 0;
        .c   PKL_PASS_SUBPASS (field_initializer);
        .c   PKL_GEN_PAYLOAD->in_constructor = 1;
        .c }
        .c else
        .c   PKL_PASS_SUBPASS (field_type);
                               ; ... SCT ENAME EVAL
.got_value:
        ;; If the field type is an array, emit a cast here so array
        ;; bounds are checked.  This is not done in promo because the
        ;; array bounders shall be evaluated in this lexical
        ;; environment.
   .c if (PKL_AST_TYPE_CODE (field_type) == PKL_TYPE_ARRAY)
   .c {
   .c   /* Make sure the cast type has a bounder.  If it doesn't */
   .c   /*   compile and install one.  */
   .c   if (PKL_AST_TYPE_A_BOUNDER (field_type) == PVM_NULL)
   .c   {
   .c      PKL_GEN_PAYLOAD->in_array_bounder = 1;
   .c      PKL_PASS_SUBPASS (field_type);
   .c      PKL_GEN_PAYLOAD->in_array_bounder = 0;
   .c    }
   .c
   .c   pkl_asm_insn (RAS_ASM, PKL_INSN_ATOA,
   .c                 NULL /* from_type */, field_type);
   .c }
        rot                    ; ... ENAME EVAL SCT
        drop                   ; ... ENAME EVAL
        dup                    ; ... ENAME EVAL EVAL
        regvar $val            ; ... ENAME EVAL
   .c   vars_registered++;
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (field);
   .c if (optcond)
   .c {
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (optcond);
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
        bnzi .optcond_ok
        drop                    ; ENAME EVAL
        drop                    ; ENAME
        drop                    ; _
        pushvar $boff            ; BOFF
        push null               ; BOFF null null
        push null               ; BOFF null null
        dup                     ; BOFF null null null
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_POPVAR,
        .c               0 /* back */, 3 + vars_registered - 1 /* over */);
        ba .omitted_field
   .c }
   .c else
   .c {
        push null               ; BOFF STR VAL null
   .c }
.optcond_ok:
        drop                    ; BOFF STR VAL
        ;; Evaluate the constraint expression.
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field) != NULL)
   .c {
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field));
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
        bnzi .constraint_ok
        drop
   .c   if (PKL_AST_TYPE_S_UNION (type_struct))
   .c   {
        ;; Alternative failed: try next alternative.
        ba .alternative_failed
   .c   }
        push PVM_E_CONSTRAINT
        raise
.constraint_ok:
        drop
   .c }
        ;; Increase off with the siz of the last element.  Note
        ;; the offset starts at 0 since this struct is not mapped,
        ;; unless the struct is pinned.
   .c if (PKL_AST_TYPE_S_PINNED (type_struct))
   .c {
        push uint<64>0         ; ... ENAME EVAL EBOFF
        dup                    ; ... ENAME EVAL EBOFF NEBOFF
   .c }
   .c else
   .c {
        siz                    ; ... ENAME EVAL ESIZ
        pushvar $boff          ; ... ENAME EVAL ESIZ EBOFF
        swap                   ; ... ENAME EVAL EBOFF ESIZ
        addlu
        nip                    ; ... ENAME EVAL EBOFF NEBOFF
   .c }
        popvar $boff           ; ... ENAME EVAL NEBOFF
        nrot                   ; ... NEBOFF ENAME EVAL
.omitted_field:
        ;; Increase the number of fields.
        pushvar $nfield        ; ... NEBOFF ENAME EVAL NFIELD
        push ulong<64>1        ; ... NEBOFF ENAME EVAL NFIELD 1
        addlu
        nip2                   ; ... NEBOFF ENAME EVAL (NFIELD+1UL)
        popvar $nfield         ; ... NEBOFF ENAME EVAL
   .c if (PKL_AST_TYPE_S_UNION (type_struct))
   .c {
        ;; Union field successfully constructed.  We are done.
        ba .union_fields_done
.alternative_failed:
        drop                    ; ... ENAME
        drop                    ; ... EVAL
   .c }
 .c }
 .c if (PKL_AST_TYPE_S_UNION (type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        ;; Handle the methods.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c   if (PKL_AST_CODE (field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (field)))
 .c   {
 .c     if (PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 3 +
        ;; element order.  This 3 should be updated if the lexical
        ;; structure of this function changes.
        ;;
        ;; XXX push the closure specified in the struct constructor
        ;; instead, if appropriate.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH,
 .c                   pvm_make_string (PKL_AST_IDENTIFIER_POINTER (PKL_AST_DECL_NAME (field))));
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 3 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (nmethod, 64));
 .c }
        ;; Push the number of fields, create the struct and return it.
        pushvar $nfield        ; null [OFF STR VAL]... NFIELD
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (type_struct);
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
                                ; null [OFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        popf 1
        return
        .end

;;; RAS_MACRO_STRUCT_FIELD_WRITER
;;; ( IOS SCT I -- )
;;;
;;; Macro that writes the Ith field of struct SCT to the given IOS.
;;;
;;; C environment required:
;;; `field' is a pkl_ast_node with the type of the field to write.

        .macro struct_field_writer
        ;; Do not write absent fields.
        srefia                  ; IOS SCT I ABSENT_P
        bnzi .omitted_field
        drop                    ; IOS SCT
        ;; The field is written out only if it hasn't
        ;; been modified since the last mapping.
        smodi                   ; IOS SCT I MODIFIED
        bzi .omitted_field
        drop                    ; IOS SCT I
        srefi                   ; IOS SCT I EVAL
        nrot                    ; IOS EVAL SCT I
        srefio                  ; IOS EVAL SCT I EBOFF
        nip2                    ; IOS EVAL EBOFF
        swap                    ; IOS EOFF EVAL
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_GEN_PAYLOAD->in_writer = 1;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (field));
        .c PKL_GEN_PAYLOAD->in_writer = 0;
        .c PKL_GEN_PAYLOAD->endian = endian; }
        ba .next
.omitted_field:
        drop                    ; IOS SCT I
        drop                    ; IOS SCT
        drop                    ; IOS
        drop                    ; _
.next:
        .end

;;; RAS_FUNCTION_STRUCT_WRITER
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped struct value.
;;;
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        .function struct_writer
        prolog
        pushf 1
        regvar $sct             ; Argument
.c { uint64_t i;
 .c for (i = 0, field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c     if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Poke this struct field, but only if it has been modified
        ;; since the last mapping.
        pushvar $sct            ; SCT
        mgetios                 ; SCT IOS
        swap                    ; IOS SCT
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (i, 64));
                                ; IOS SCT I
        .e struct_field_writer
 .c   i = i + 1;
 .c }
.c }
        popf 1
        push null
        return
        .end
