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

;;; RAS_FUNCTION_ARRAY_MAPPER @array_type
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
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the array type being mapped.

        .function array_mapper @array_type
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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

;;; RAS_FUNCTION_ARRAY_VALMAPPER @array_type
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
;;;
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the array type being mapped.

        .function array_valmapper @array_type
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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

;;; RAS_FUNCTION_ARRAY_WRITER @array_type
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped array value.
;;;
;;; Note that it is important for the elements of the array to be
;;; poked in order.
;;;
;;; Macro arguments:
;;;
;;; @array_type is an AST node with the type of the array being
;;; written.

        .function array_writer @array_type
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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

;;; RAS_FUNCTION_ARRAY_BOUNDER @array_type
;;; ( _ -- BOUND )
;;;
;;; Assemble a function that returns the boundary of an array type.
;;; If the array type is not bounded by either number of elements nor size
;;; then PVM_NULL is returned.
;;;
;;; Note how this function doesn't introduce any lexical level.  This
;;; is important, so keep it this way!
;;;
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the type of ARR.

        .function array_bounder @array_type
        prolog
        .c if (PKL_AST_TYPE_A_BOUND (@array_type))
        .c {
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 0;
        .c   PKL_PASS_SUBPASS (PKL_AST_TYPE_A_BOUND (@array_type)) ;
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 1;
        .c }
        .c else
             push null
        return
        .end

;;; RAS_FUNCTION_ARRAY_CONSTRUCTOR @array_type
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
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the array type being constructed.

        .function array_constructor @array_type
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
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

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_LABEL @field
;;; ( BOFF SBOFF - BOFF )
;;;
;;; Given a struct type element, it's offset and the offset of the struct
;;; on the stack, increase the bit-offset by the element's label, in
;;; case it exists.
;;;
;;; Macro arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.

        .macro handle_struct_field_label @field
   .c if (PKL_AST_STRUCT_TYPE_FIELD_LABEL (@field) == NULL)
        drop                    ; BOFF
   .c else
   .c {
        nip                     ; SBOFF
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_LABEL (@field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; SBOFF LOFF
        ;; Note that this relies on the field label offset to
        ;; be offset<uint<64>,b>.  This is guaranteed by promo.
        ogetm                   ; SBOFF LOFF LOFFM
        nip                     ; SBOFF LOFFM
        addlu
        nip2                    ; (SBOFF+LOFFM)
   .c }
        .end

;;; If `field' is an optional field, evaluate the optcond.  If it is
;;; false, then add an absent field, i.e. both the field name and
;;; the field value are PVM_NULL.

;;; RAS_MACRO_CHECK_STRUCT_FIELD_CONSTRAINT @field
;;; ( -- )
;;;
;;; Evaluate the given struct field's constraint, raising an
;;; exception if not satisfied.
;;;
;;; Macro arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.

        .macro check_struct_field_constraint @field
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field) != NULL)
   .c {
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
        bnzi .constraint_ok
        drop
        push PVM_E_CONSTRAINT
        raise
.constraint_ok:
        drop
   .c }
        .end

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_CONSTRAINTS @field
;;; ( BOFF STR VAL -- BOFF STR VAL NBOFF )
;;;
;;; Given a `field', evaluate its optcond and integrity constraints,
;;; then calculate the bit-offset of the next struct value.
;;;
;;; Macro-arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro handle_struct_field_constraints @field
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field);
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
        ;; Note that this 4 should be updated if the lexical structure
        ;; of struct_mapper and struct_constructor changes!
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_POPVAR,
        .c               0 /* back */, 4 + vars_registered - 1 /* over */);
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
        .e check_struct_field_constraint @field
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

;;; RAS_MACRO_STRUCT_FIELD_EXTRACTOR
;;;               field struct_itype field_type ivalw fieldw
;;; ( BOFF SBOFF IVAL -- BOFF STR VAL NBOFF )
;;;
;;; Given an integer large enough, extract the value of the given field
;;; from it.
;;;
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;; by this macro.  It is typically ulong<64>0 or ulong<64>1.
;;;
;;; Macro-arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; @struct_itype is the AST node with the type of the struct being
;;; processed.
;;;
;;; @field_type is the AST node with the type of the field being
;;; extracted.
;;;
;;; #ivalw is an ulong<64> value with the width (in bits) of the
;;; integral value corresponding to the entire integral struct.
;;;
;;; #fieldw is an ulong<64> value with the width (in bits) of the
;;; field being extracted.
;;;
;;; The C environment required is:
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro struct_field_extractor @field @struct_itype @field_type #ivalw #fieldw
        nrot                            ; IVAL BOFF SBOFF
        ;; Calculate the amount of bits that we have to right-shift
        ;; IVAL in order to extract the portion of the value
        ;; corresponding to this field.  The formula is:
        ;;
        ;; (ival_width - field_width) - (field_offset - struct_offset)
        ;;
        over                            ; IVAL BOFF SBOFF BOFF
        swap                            ; IVAL BOFF BOFF SBOFF
        sublu
        nip2                            ; IVAL BOFF (BOFF-SBOFF)
        push #ivalw                     ; IVAL BOFF (BOFF-SBOFF) IVALW
        push #fieldw                    ; IVAL BOFF (BOFF-SBOFF) IVALW FIELDW
        sublu
        nip2                            ; IVAL BOFF (BOFF-SBOFF) (IVALW-FIELDW)
        swap                            ; IVAL BOFF (IVALW-FIELDW) (BOFF-SBOFF)
        sublu
        nip2                            ; IVAL BOFF ((IVALW-FIELDW)-(BOFF-SBOFF))
        quake                           ; BOFF IVAL SCOUNT
        lutoiu 32
        nip                             ; BOFF IVAL SCOUNT(U)
        ;; Using the calculated bit-count, extract the value of the
        ;; field from the struct ival.  The resulting value is converted
        ;; to the type of the field. (base type if the field is offset.)
        sr @struct_itype
        nip2                            ; BOFF VAL
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
   .c {
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@field_type)
        nton @struct_itype, @base_type
   .c }
   .c else
   .c {
        nton @struct_itype, @field_type
   .c }
        nip                             ; BOFF VALC
        ;; At this point we have either the value of the field is in the
        ;; stack.  If the field is an offset, construct it.
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
   .c {
        .let @offset_unit = PKL_AST_TYPE_O_UNIT (@field_type)
        .let #unit = pvm_make_ulong (PKL_AST_INTEGER_VALUE (@offset_unit), 64)
        push #unit                      ; BOFF MVALC UNIT
        mko                             ; BOFF VALC
   .c }
        dup                             ; BOFF VALC VALC
        regvar $val                     ; BOFF VALC
        .c vars_registered++;
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field));
                                        ; BOFF VALC STR
        swap                            ; BOFF STR VALC
        ;; Evaluate the field's opcond and constraints
        .e handle_struct_field_constraints @field
                                        ; BOFF STR VALC NBOFF
        .end

;;; RAS_MACRO_STRUCT_FIELD_MAPPER
;;; ( IOS BOFF SBOFF -- BOFF STR VAL NBOFF )
;;;
;;; Map a struct field from the current IOS.
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;; by this macro.  It is typically ulong<64>0 or ulong<64>1.
;;;
;;; Macro-arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; Required C environment:
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro struct_field_mapper @field
        ;; Increase OFF by the label, if the field has one.
        .e handle_struct_field_label @field
                                        ; IOS BOFF
        dup                             ; IOS BOFF BOFF
        nrot                            ; BOFF IOS BOFF
        push PVM_E_CONSTRAINT
        pushe .constraint_error_or_eof
        push PVM_E_EOF
        pushe .constraint_error_or_eof
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field));
        .c PKL_GEN_PAYLOAD->endian = endian; }
                                        ; BOFF VAL
        pope
        pope
        ba .val_ok
.constraint_error_or_eof:
        ;; This is to keep the right lexical environment in
        ;; case the subpass above raises a constraint exception.
        push null
        regvar $val
        raise
.val_ok:
        dup                             ; BOFF VAL VAL
        regvar $val                     ; BOFF VAL
        .c vars_registered++;
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field));
                                        ; BOFF VAL STR
        swap                            ; BOFF STR VAL
        ;; Evaluate the field's opcond and constraints
        .e handle_struct_field_constraints @field
                                        ; BOFF STR VAL NBOFF
        .end

;;; RAS_FUNCTION_STRUCT_MAPPER @type_struct
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
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        ;; NOTE: please be careful when altering the lexical structure of
        ;; this code (and of the code in expanded macros). Every local
        ;; added should be also reflected in the compile-time environment
        ;; in pkl-tab.y, or horrible things _will_ happen.  So if you
        ;; add/remove locals here, adjust accordingly in
        ;; pkl-tab.y:struct_type_specifier.  Thank you very mucho!

        .function struct_mapper @type_struct
        prolog
        pushf 4
        drop                    ; sbound
        drop                    ; ebound
        regvar $boff
        regvar $ios
        push ulong<64>0
        regvar $nfield
        ;; If the struct is integral, map the integer from which the
        ;; value of the fields will be derived.  Otherwise, just register
        ;; a dummy value that will never be used.
  .c if (PKL_AST_TYPE_S_ITYPE (@type_struct))
  .c {
        pushvar $ios
        pushvar $boff
  .c    PKL_PASS_SUBPASS (PKL_AST_TYPE_S_ITYPE (@type_struct));
  .c }
  .c else
  .c {
        push null
  .c }
        regvar $ivalue
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        ;; Iterate over the elements of the struct type.
        .let @field
 .c size_t vars_registered = 0;
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_mapper = 0;
 .c     PKL_PASS_SUBPASS (@field);
 .c     PKL_GEN_PAYLOAD->in_mapper = 1;
 .c
 .c     continue;
 .c   }
        .label .alternative_failed
        .label .eof_in_alternative
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        push PVM_E_EOF
        pushe .eof_in_alternative
        push PVM_E_CONSTRAINT
        pushe .alternative_failed
 .c   }
        pushvar $ios             ; ...[EBOFF ENAME EVAL] NEBOFF IOS
        swap                     ; ...[EBOFF ENAME EVAL] IOS NEBOFF
        pushvar $boff            ; ...[EBOFF ENAME EVAL] IOS NEBOFF OFF
 .c   if (PKL_AST_TYPE_S_ITYPE (@type_struct))
 .c   {
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct);
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        ;; Note that at this point the field is assured to be
        ;; an integral type, as per typify.
        rot                      ; ...[EBOFF ENAME EVAL] NEBOFF OFF IOS
        drop                     ; ...[EBOFF ENAME EVAL] NEBOFF OFF
        pushvar $ivalue          ; ...[EBOFF ENAME EVAL] NEBOFF OFF IVAL
        .e struct_field_extractor @field, @struct_itype, @field_type, #ivalw, #fieldw
                                 ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c   else
 .c   {
        .e struct_field_mapper @field
                                ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
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
 .c   if (PKL_AST_TYPE_S_PINNED_P (@type_struct))
 .c   {
        drop
        pushvar $boff           ; ...[EBOFF ENAME EVAL] BOFF
 .c   }
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        ;; Union field successfully mapped.
        ;;
        ;; Now we need to register as many dummies in the lexical
        ;; environment as remaining alternatives, and also definitions
        ;; and methods, in order to obtain predictable lexical addresses
        .let @tmp = PKL_AST_CHAIN (@field);
 .c    for (; @tmp; @tmp = PKL_AST_CHAIN (@tmp))
 .c    {
 .c      if (PKL_AST_CODE (@tmp) == PKL_AST_STRUCT_TYPE_FIELD)
 .c      {
        push null
        regvar $dummy
 .c      }
 .c      else
 .c      {
 .c     PKL_GEN_PAYLOAD->in_mapper = 0;
 .c     PKL_PASS_SUBPASS (@tmp);
 .c     PKL_GEN_PAYLOAD->in_mapper = 1;
 .c      }
 .c    }
        ;; And we are done.
        ba .union_fields_done
.eof_in_alternative:
        ;; If we got EOF in an union alternative, and this is the last
        ;; alternative in the union, re-raise it.  Otherwise just
        ;; try the next alternative.
     .c if (PKL_AST_CHAIN (@field) == NULL)
     .c {
        raise
     .c }
.alternative_failed:
        ;; Drop the exception and try next alternative.
        drop                    ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c }
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
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
 .c for (nmethod = 0, i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@field)))
 .c   {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c         || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 4 +
        ;; element order.  This 4 should be updated if the lexical
        ;; structure of this function changes.
        .let @decl_name = PKL_AST_DECL_NAME (@field)
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@decl_name))
        push #name_str
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 4 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
        .let #nmethods = pvm_make_ulong (nmethod, 64)
        push #nmethods
 .c }
        ;;  Push the number of fields
        pushvar $nfield         ; BOFF [EBOFF STR VAL]... NFIELD
        ;; Finally, push the struct type and call mksct.
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (@type_struct);
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; BOFF [EBOFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        ;; Install the attributes of the mapped object.
        pushvar $ios            ; SCT IOS
        msetios                 ; SCT
        popf 1
        return
        .end

;;; RAS_FUNCTION_STRUCT_COMPARATOR @type_struct
;;; ( SCT SCT -- INT )
;;;
;;; Assemble a function that, given two structs of a given type,
;;; returns 1 if the two structs are equal, 0 otherwise.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        ;; NOTE: this function should have the same lexical structure
        ;; than struct_mapper above.  If you add more local variables,
        ;; please adjust struct_comparator accordingly, and also follow
        ;; the instructions on the NOTE there.

        .function struct_comparator @type_struct
        prolog
 .c { uint64_t i;
        .let @field
 .c  for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c       @field;
 .c       @field = PKL_AST_CHAIN (@field), ++i)
 .c  {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Compare the fields of both structs.
        tor                     ; SCT1 [SCT2]
        push #i                 ; SCT1 I [SCT2]
        srefi                   ; SCT1 I VAL1 [SCT2]
        swap                    ; SCT1 VAL1 I [SCT2]
        fromr                   ; SCT1 VAL1 I SCT2
        swap                    ; SCT1 VAL1 SCT2 I
        srefi                   ; SCT1 VAL1 SCT2 I VAL2
        nip                     ; SCT1 VAL1 SCT2 VAL2
        quake                   ; SCT1 SCT2 VAL1 VAL2
 .c if (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field))
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
 .c     pkl_ast_node field_type
 .c       = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field) ;
 .c     if (PKL_AST_TYPE_CODE (field_type) == PKL_TYPE_STRUCT)
 .c       PKL_PASS_SUBPASS (field_type);
 .c     else
 .c     {
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        eq @field_type
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

;;; RAS_MACRO_STRUCT_FIELD_CONSTRUCTOR
;;; ( SCT -- VAL INT )
;;;
;;; Construct a struct field, given an initial SCT that may be NULL.
;;; Push on the stack the constructed value, and an integer predicate
;;; indicating whether the construction of the value was successful,
;;; or resulted in a constraint error.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is an AST node denoting the type of the struct
;;; being constructed.
;;;
;;; @field_type is an AST node denoting the type of the field to
;;; construct.

        .macro struct_field_constructor @type_struct @field_type
                                ; SCT
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        push PVM_E_CONSTRAINT
        pushe .constraint_error
 .c }
 .c     PKL_PASS_SUBPASS (@field_type);
                                ; VAL
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        pope
        ba .val_ok
.constraint_error:
        ;; This is to keep the right lexical environment in case the
        ;; subpass above raises a constraint exception.
        drop
        push null
        regvar $val
        push int<32>0           ; 0
        ba .done
.val_ok:
  .c }
        push int<32>1           ; VAL 1
.done:
        .end

;;; RAS_FUNCTION_STRUCT_CONSTRUCTOR @type_struct
;;; ( SCT -- SCT )
;;;
;;; Assemble a function that constructs a struct value of a given type
;;; from another struct value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        ;; NOTE: this function should have the same lexical structure
        ;; than struct_mapper above.  If you add more local variables,
        ;; please adjust struct_mapper accordingly, and also follow the
        ;; instructions on the NOTE there.

        .function struct_constructor @type_struct
        prolog
        pushf 4
        regvar $sct             ; SCT
        ;; Initialize $nfield to 0UL
        push ulong<64>0
        regvar $nfield
        ;; Initialize $boff to 0UL#b.
        push ulong<64>0
        regvar $boff
        ;; So we have the same lexical structure than struct_mapper.
        push null
        regvar $unused
        ;; The struct is not mapped, so its offset is null.
        push null               ; null
        ;; Iterate over the fields of the struct type.
 .c size_t vars_registered = 0;
        .let @field
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .label .alternative_failed
        .label .constraint_ok
        .label .optcond_ok
        .label .omitted_field
        .label .got_value
        .let @field_initializer = PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (@field)
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
 .c   if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_constructor = 0;
 .c     PKL_PASS_SUBPASS (@field);
 .c     PKL_GEN_PAYLOAD->in_constructor = 1;
 .c
 .c     continue;
 .c   }
        pushvar $sct           ; ... [EBOFF ENAME EVAL] SCT
 .c   pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field);
 .c   if (field_name)
 .c   {
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (field_name))
        push #field_name_str   ; ... SCT ENAME
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
 .c if (@field_initializer)
 .c {
        drop
 .c     PKL_GEN_PAYLOAD->in_constructor = 0;
 .c     PKL_PASS_SUBPASS (@field_initializer);
 .c     PKL_GEN_PAYLOAD->in_constructor = 1;
 .c }
 .c else
 .c {
        .e struct_field_constructor @type_struct, @field_type ; SCT ENAME EVAL INT
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        .label .alternative_ok
        bnzi .alternative_ok
        drop
        drop
        ba .alternative_failed
.alternative_ok:
 .c   }
        drop                                    ; SCT ENAME EVAL
  .c }
.got_value:
        ;; If the field type is an array, emit a cast here so array
        ;; bounds are checked.  This is not done in promo because the
        ;; array bounders shall be evaluated in this lexical
        ;; environment.
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_ARRAY)
   .c {
   .c   /* Make sure the cast type has a bounder.  If it doesn't */
   .c   /*   compile and install one.  */
   .c   if (PKL_AST_TYPE_A_BOUNDER (@field_type) == PVM_NULL)
   .c   {
   .c      PKL_GEN_PAYLOAD->in_array_bounder = 1;
   .c      PKL_PASS_SUBPASS (@field_type);
   .c      PKL_GEN_PAYLOAD->in_array_bounder = 0;
   .c    }
   .c
   .c   pkl_asm_insn (RAS_ASM, PKL_INSN_ATOA,
   .c                 NULL /* from_type */, @field_type);
   .c }
        rot                    ; ... ENAME EVAL SCT
        drop                   ; ... ENAME EVAL
        dup                    ; ... ENAME EVAL EVAL
        regvar $val            ; ... ENAME EVAL
   .c   vars_registered++;
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field);
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
        .c               0 /* back */, 4 + vars_registered - 1 /* over */);
        ba .omitted_field
   .c }
   .c else
   .c {
        push null               ; BOFF STR VAL null
   .c }
.optcond_ok:
        drop                    ; BOFF STR VAL
        ;; Evaluate the constraint expression.
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field) != NULL)
   .c {
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field));
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
        bnzi .constraint_ok
        drop
   .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
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
   .c if (PKL_AST_TYPE_S_PINNED_P (@type_struct))
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
   .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
   .c {
        ;; Union field successfully constructed.
        ;;
        ;; Now we need to register as many dummies in the lexical
        ;; environment as remaining alternatives, and also definitions
        ;; and methods, in order to obtain predictable lexical addresses
        .let @tmp = PKL_AST_CHAIN (@field);
 .c    for (; @tmp; @tmp = PKL_AST_CHAIN (@tmp))
 .c    {
 .c      if (PKL_AST_CODE (@tmp) == PKL_AST_STRUCT_TYPE_FIELD)
 .c      {
        push null
        regvar $dummy
 .c      }
 .c      else
 .c      {
 .c     PKL_GEN_PAYLOAD->in_constructor = 0;
 .c     PKL_PASS_SUBPASS (@tmp);
 .c     PKL_GEN_PAYLOAD->in_constructor = 1;
 .c      }
 .c    }
        ;; And we are done :)
        ba .union_fields_done
.alternative_failed:
        drop                    ; ... ENAME
        drop                    ; ... EVAL
   .c }
 .c }
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        ;; Handle the methods.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@field)))
 .c   {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c         || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 4 +
        ;; element order.  This 4 should be updated if the lexical
        ;; structure of this function changes.
        ;;
        ;; XXX push the closure specified in the struct constructor
        ;; instead, if appropriate.
        .let @decl_name = PKL_AST_DECL_NAME (@field)
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@decl_name))
        push #name_str
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 4 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
        .let #nmethod = pvm_make_ulong (nmethod, 64)
        push #nmethod
 .c }
        ;; Push the number of fields, create the struct and return it.
        pushvar $nfield        ; null [OFF STR VAL]... NFIELD
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (@type_struct);
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
                                ; null [OFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        popf 1
        return
        .end

;;; RAS_MACRO_STRUCT_FIELD_INSERTER
;;;                       @struct_itype @field_type #ivalw #fieldw
;;; ( IVAL SCT I -- NIVAL )
;;;
;;; Macro that given a struct, a field index and an ival, inserts
;;; the value of the field in the ival and pushes the new ival.
;;;
;;; Macro-arguments:
;;;
;;; @struct_itype is the AST node with the type of the struct being
;;; processed.
;;;
;;; @field_type is the AST node with the type of the field being
;;; extracted.
;;;
;;; #ivalw is an ulong<64> value with the width (in bits) of the
;;; integral value corresponding to the entire integral struct.
;;;
;;; #fieldw is an ulong<64> value with the width (in bits) of the
;;; field being extracted.

        .macro struct_field_inserter @struct_itype @field_type #ivalw #fieldw
        ;; Do not insert absent fields.
        srefia                  ; IVAL SCT I ABSENT_P
        bnzi .omitted_field
        drop                    ; IVAL SCT I
        ;; Insert the value of the field in IVAL:
        ;;
        ;; IVAL = IVAL | (EVAL << (ival_width - REOFF - field_width))
        ;;
        ;;  Where REOFF is the _relative_ bit-offset of the field in
        ;;  the struct, i.e. EOFF - SOFF
        rot                     ; SCT I IVAL
        tor                     ; SCT I [IVAL]
        srefi                   ; SCT I EVAL [IVAL]
        tor                     ; SCT I [IVAL EVAL]
        swap                    ; I SCT [IVAL EVAL]
        mgeto                   ; I SCT SOFF [IVAL EVAL]
        ;; XXX: if NULL, put 0UL.  This check will go away when we
        ;; add a separated flag for mapped values.
        bnn .done
        drop
        push ulong<64>0
.done:
        tor                     ; I SCT [IVAL EVAL SOFF]
        swap                    ; SCT I [IVAL EVAL SOFF]
        srefio                  ; SCT I EOFF [IVAL EVAL SOFF]
        fromr                   ; SCT I EOFF SOFF [IVAL EVAL]
        sublu
        nip2                    ; SCT I (EOFF-SOFF) [IVAL EVAL]
        push #ivalw             ; SCT I REOFF IVALW [IVAL EVAL]
        swap                    ; SCT I IVALW REOFF [IVAL EVAL]
        sublu
        nip2                    ; SCT I (IVALW-REOFF) [IVAL EVAL]
        push #fieldw            ; SCT I EVAL (IVALW-REOFF) FIELDW [IVAL EVAL]
        sublu
        nip2                    ; SCT I EVAL (IVALW-REOFF-FIELDW) [IVAL EVAL]
        ;; Convert EVAL to the struct itype
        fromr                   ; SCT I (IVALW-REOFF-FIELDW) EVAL [IVAL]
 .c   if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
 .c   {
        ;; EVAL is an offset, but we are interested in its magnitude.
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@field_type)
        ogetm
        nip                     ; SCT I (IVALW-REOFF-FIELDW) EVAL [IVAL]
        nton @base_type, @struct_itype
 .c   }
 .c   else
 .c   {
        nton @field_type, @struct_itype
 .c   }
        nip                     ; SCT I (IVALW-EOFF-FIELDW) EVAL [IVAL]
        swap                    ; SCT I EVAL (IVALW-EOFF-FIELDW) [IVAL]
        lutoiu 32
        nip                     ; SCT I EVAL (IVALW-EOFF-FIELDW) [IVAL]
        sl @struct_itype
        nip2                    ; SCT I (EVAL<<(IVALW-EOFF-FIELDW)) [IVAL]
        fromr                   ; SCT I (EVAL<<(IVALW-EOFF-FIELDW)) IVAL
        bor @struct_itype
        nip2                    ; SCT I ((EVAL<<(IVALW-EOFF-FIELDW))|IVAL)
        nip2                    ; ((EVAL<<(IVALW-EOFF-FIELDW))|IVAL)
        ba .next
.omitted_field:
        ;; Field ommitted => IVAL stays unmodified.
        ;; XXX this doens't work with absent fields, as it results
        ;; on zeroes in the portions occupied by the absent field.
        drop                    ; IVAL SCT I
        drop                    ; IVAL SCT
        drop                    ; IVAL
.next:
        .end

;;; RAS_MACRO_STRUCT_FIELD_WRITER @field
;;; ( IOS SCT I -- )
;;;
;;; Macro that writes the Ith field of struct SCT to the given IOS.
;;;
;;; Macro-arguments:
;;;
;;; @field is a pkl_ast_node with the type of the field to write.

        .macro struct_field_writer @field
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
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PAYLOAD->in_writer = 1;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field));
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

;;; RAS_FUNCTION_STRUCT_WRITER @type_struct
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped struct value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        .function struct_writer @type_struct
        prolog
        pushf 2
        regvar $sct             ; Argument
        ;; If the struct is integral, initialize $ivalue to
        ;; 0, of the corresponding type.
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct)
        push null
  .c if (@struct_itype)
  .c {
        ;; Note that the constructor consumes the null
        ;; on the stack.
  .c    PKL_GEN_PAYLOAD->in_writer = 0;
  .c    PKL_GEN_PAYLOAD->in_constructor = 1;
  .c    PKL_PASS_SUBPASS (@struct_itype);
  .c    PKL_GEN_PAYLOAD->in_constructor = 0;
  .c    PKL_GEN_PAYLOAD->in_writer = 1;
  .c }
        regvar $ivalue
 .c {
 .c      uint64_t i;
        .let @field
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Poke this struct field, but only if it has been modified
        ;; since the last mapping.
        pushvar $sct            ; SCT
        push #i                 ; SCT I
 .c  if (@struct_itype)
 .c  {
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        pushvar $ivalue          ; SCT I IVAL
        nrot                     ; IVAL SCT I
        .e struct_field_inserter @struct_itype, @field_type, #ivalw, #fieldw
                                 ; NIVAL
        popvar $ivalue           ; _
 .c  }
 .c  else
 .c  {
        swap                    ; I SCT
        mgetios                 ; I SCT IOS
        swap                    ; I IOS SCT
        rot                     ; IOS SCT I
        .e struct_field_writer @field
                                ; _
 .c  }
 .c    i = i + 1;
 .c }
        .c }
        ;; If the struct is integral, poke the resulting ival.
 .c if (@struct_itype)
 .c {
        pushvar $sct            ; SCT
        mgetios                 ; SCT IOS
        nip                     ; IOS
        push ulong<64>0         ; IOS 0UL
        pushvar $ivalue         ; IOS 0UL IVAL
 .c     PKL_PASS_SUBPASS (@struct_itype);
 .c }
        popf 1
        push null
        return
        .end

;;; RAS_FUNCTION_STRUCT_INTEGRATOR @type_struct
;;; ( VAL -- IVAL )
;;;
;;; Assemble a function that, given an integral struct, returns
;;; it's integral value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the type of the struct
;;; passed in the stack.

        .function struct_integrator @type_struct
        prolog
        pushf 2
        regvar $sct             ; Argument
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct)
        push null
        ;; Note that the constructor consumes the null
        ;; on the stack.
  .c    PKL_GEN_PAYLOAD->in_writer = 0;
  .c    PKL_GEN_PAYLOAD->in_constructor = 1;
  .c    PKL_PASS_SUBPASS (@struct_itype);
  .c    PKL_GEN_PAYLOAD->in_constructor = 0;
  .c    PKL_GEN_PAYLOAD->in_writer = 1;
        regvar $ivalue
        .let @field
 .c      uint64_t i;
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        pushvar $sct            ; SCT
        push #i                 ; SCT I
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        pushvar $ivalue          ; SCT I IVAL
        nrot                     ; IVAL SCT I
        .e struct_field_inserter @struct_itype, @field_type, #ivalw, #fieldw
                                 ; NIVAL
        popvar $ivalue           ; _
 .c     i = i + 1;
 .c }
        pushvar $ivalue
        popf 1
        return
        .end
