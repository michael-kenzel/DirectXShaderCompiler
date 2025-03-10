//===--- SemaHLSL.cpp       - HLSL support for AST nodes and operations ---===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaHLSL.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file implements the semantic support for HLSL.                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/DenseMap.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExternalASTSource.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/TypeLoc.h"
#include "clang/AST/HlslTypes.h"
#include "clang/Sema/Overload.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/ExternalSemaSource.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/TemplateDeduction.h"
#include "clang/Sema/SemaHLSL.h"
#include "clang/Sema/SemaHLSL.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/WinAdapter.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/HlslIntrinsicOp.h"
#include "gen_intrin_main_tables_15.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include <array>
#include <float.h>

enum ArBasicKind {
  AR_BASIC_BOOL,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_FLOAT16,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT64,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_INT8,
  AR_BASIC_UINT8,
  AR_BASIC_INT16,
  AR_BASIC_UINT16,
  AR_BASIC_INT32,
  AR_BASIC_UINT32,
  AR_BASIC_INT64,
  AR_BASIC_UINT64,

  AR_BASIC_MIN10FLOAT,
  AR_BASIC_MIN16FLOAT,
  AR_BASIC_MIN12INT,
  AR_BASIC_MIN16INT,
  AR_BASIC_MIN16UINT,
  AR_BASIC_ENUM,

  AR_BASIC_COUNT,

  //
  // Pseudo-entries for intrinsic tables and such.
  //

  AR_BASIC_NONE,
  AR_BASIC_UNKNOWN,
  AR_BASIC_NOCAST,

  //
  // The following pseudo-entries represent higher-level
  // object types that are treated as units.
  //

  AR_BASIC_POINTER,
  AR_BASIC_ENUM_CLASS,

  AR_OBJECT_NULL,
  AR_OBJECT_STRING_LITERAL,
  AR_OBJECT_STRING,

  // AR_OBJECT_TEXTURE,
  AR_OBJECT_TEXTURE1D,
  AR_OBJECT_TEXTURE1D_ARRAY,
  AR_OBJECT_TEXTURE2D,
  AR_OBJECT_TEXTURE2D_ARRAY,
  AR_OBJECT_TEXTURE3D,
  AR_OBJECT_TEXTURECUBE,
  AR_OBJECT_TEXTURECUBE_ARRAY,
  AR_OBJECT_TEXTURE2DMS,
  AR_OBJECT_TEXTURE2DMS_ARRAY,

  AR_OBJECT_SAMPLER,
  AR_OBJECT_SAMPLER1D,
  AR_OBJECT_SAMPLER2D,
  AR_OBJECT_SAMPLER3D,
  AR_OBJECT_SAMPLERCUBE,
  AR_OBJECT_SAMPLERCOMPARISON,

  AR_OBJECT_BUFFER,

  //
  // View objects are only used as variable/types within the Effects
  // framework, for example in calls to OMSetRenderTargets.
  //

  AR_OBJECT_RENDERTARGETVIEW,
  AR_OBJECT_DEPTHSTENCILVIEW,

  //
  // Shader objects are only used as variable/types within the Effects
  // framework, for example as a result of CompileShader().
  //

  AR_OBJECT_COMPUTESHADER,
  AR_OBJECT_DOMAINSHADER,
  AR_OBJECT_GEOMETRYSHADER,
  AR_OBJECT_HULLSHADER,
  AR_OBJECT_PIXELSHADER,
  AR_OBJECT_VERTEXSHADER,
  AR_OBJECT_PIXELFRAGMENT,
  AR_OBJECT_VERTEXFRAGMENT,

  AR_OBJECT_STATEBLOCK,

  AR_OBJECT_RASTERIZER,
  AR_OBJECT_DEPTHSTENCIL,
  AR_OBJECT_BLEND,

  AR_OBJECT_POINTSTREAM,
  AR_OBJECT_LINESTREAM,
  AR_OBJECT_TRIANGLESTREAM,

  AR_OBJECT_INPUTPATCH,
  AR_OBJECT_OUTPUTPATCH,

  AR_OBJECT_RWTEXTURE1D,
  AR_OBJECT_RWTEXTURE1D_ARRAY,
  AR_OBJECT_RWTEXTURE2D,
  AR_OBJECT_RWTEXTURE2D_ARRAY,
  AR_OBJECT_RWTEXTURE3D,
  AR_OBJECT_RWBUFFER,

  AR_OBJECT_BYTEADDRESS_BUFFER,
  AR_OBJECT_RWBYTEADDRESS_BUFFER,
  AR_OBJECT_STRUCTURED_BUFFER,
  AR_OBJECT_RWSTRUCTURED_BUFFER,
  AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC,
  AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME,
  AR_OBJECT_APPEND_STRUCTURED_BUFFER,
  AR_OBJECT_CONSUME_STRUCTURED_BUFFER,

  AR_OBJECT_CONSTANT_BUFFER,
  AR_OBJECT_TEXTURE_BUFFER,

  AR_OBJECT_ROVBUFFER,
  AR_OBJECT_ROVBYTEADDRESS_BUFFER,
  AR_OBJECT_ROVSTRUCTURED_BUFFER,
  AR_OBJECT_ROVTEXTURE1D,
  AR_OBJECT_ROVTEXTURE1D_ARRAY,
  AR_OBJECT_ROVTEXTURE2D,
  AR_OBJECT_ROVTEXTURE2D_ARRAY,
  AR_OBJECT_ROVTEXTURE3D,

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  AR_OBJECT_VK_SUBPASS_INPUT,
  AR_OBJECT_VK_SUBPASS_INPUT_MS,
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  AR_OBJECT_INNER,       // Used for internal type object

  AR_OBJECT_LEGACY_EFFECT,

  AR_OBJECT_WAVE,

  AR_OBJECT_RAY_DESC,
  AR_OBJECT_ACCELERATION_STRUCT,
  AR_OBJECT_USER_DEFINED_TYPE,
  AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES,

  // subobjects
  AR_OBJECT_STATE_OBJECT_CONFIG,
  AR_OBJECT_GLOBAL_ROOT_SIGNATURE,
  AR_OBJECT_LOCAL_ROOT_SIGNATURE,
  AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC,
  AR_OBJECT_RAYTRACING_SHADER_CONFIG,
  AR_OBJECT_RAYTRACING_PIPELINE_CONFIG,
  AR_OBJECT_TRIANGLE_HIT_GROUP,
  AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP,

  AR_BASIC_MAXIMUM_COUNT
};

#define AR_BASIC_TEXTURE_MS_CASES \
    case AR_OBJECT_TEXTURE2DMS: \
    case AR_OBJECT_TEXTURE2DMS_ARRAY

#define AR_BASIC_NON_TEXTURE_MS_CASES \
    case AR_OBJECT_TEXTURE1D: \
    case AR_OBJECT_TEXTURE1D_ARRAY: \
    case AR_OBJECT_TEXTURE2D: \
    case AR_OBJECT_TEXTURE2D_ARRAY: \
    case AR_OBJECT_TEXTURE3D: \
    case AR_OBJECT_TEXTURECUBE: \
    case AR_OBJECT_TEXTURECUBE_ARRAY

#define AR_BASIC_TEXTURE_CASES \
    AR_BASIC_TEXTURE_MS_CASES: \
    AR_BASIC_NON_TEXTURE_MS_CASES

#define AR_BASIC_NON_CMP_SAMPLER_CASES \
    case AR_OBJECT_SAMPLER: \
    case AR_OBJECT_SAMPLER1D: \
    case AR_OBJECT_SAMPLER2D: \
    case AR_OBJECT_SAMPLER3D: \
    case AR_OBJECT_SAMPLERCUBE

#define AR_BASIC_ROBJECT_CASES \
    case AR_OBJECT_BLEND: \
    case AR_OBJECT_RASTERIZER: \
    case AR_OBJECT_DEPTHSTENCIL: \
    case AR_OBJECT_STATEBLOCK

//
// Properties of entries in the ArBasicKind enumeration.
// These properties are intended to allow easy identification
// of classes of basic kinds.  More specific checks on the
// actual kind values could then be done.
//

// The first four bits are used as a subtype indicator,
// such as bit count for primitive kinds or specific
// types for non-primitive-data kinds.
#define BPROP_SUBTYPE_MASK      0x0000000f

// Bit counts must be ordered from smaller to larger.
#define BPROP_BITS0             0x00000000
#define BPROP_BITS8             0x00000001
#define BPROP_BITS10            0x00000002
#define BPROP_BITS12            0x00000003
#define BPROP_BITS16            0x00000004
#define BPROP_BITS32            0x00000005
#define BPROP_BITS64            0x00000006
#define BPROP_BITS_NON_PRIM     0x00000007

#define GET_BPROP_SUBTYPE(_Props) ((_Props) & BPROP_SUBTYPE_MASK)
#define GET_BPROP_BITS(_Props)    ((_Props) & BPROP_SUBTYPE_MASK)

#define BPROP_BOOLEAN           0x00000010  // Whether the type is bool
#define BPROP_INTEGER           0x00000020  // Whether the type is an integer
#define BPROP_UNSIGNED          0x00000040  // Whether the type is an unsigned numeric (its absence implies signed)
#define BPROP_NUMERIC           0x00000080  // Whether the type is numeric or boolean
#define BPROP_LITERAL           0x00000100  // Whether the type is a literal float or integer
#define BPROP_FLOATING          0x00000200  // Whether the type is a float
#define BPROP_OBJECT            0x00000400  // Whether the type is an object (including null or stream)
#define BPROP_OTHER             0x00000800  // Whether the type is a pseudo-entry in another table.
#define BPROP_PARTIAL_PRECISION 0x00001000  // Whether the type has partial precision for calculations (i.e., is this 'half')
#define BPROP_POINTER           0x00002000  // Whether the type is a basic pointer.
#define BPROP_TEXTURE           0x00004000  // Whether the type is any kind of texture.
#define BPROP_SAMPLER           0x00008000  // Whether the type is any kind of sampler object.
#define BPROP_STREAM            0x00010000  // Whether the type is a point, line or triangle stream.
#define BPROP_PATCH             0x00020000  // Whether the type is an input or output patch.
#define BPROP_RBUFFER           0x00040000  // Whether the type acts as a read-only buffer.
#define BPROP_RWBUFFER          0x00080000  // Whether the type acts as a read-write buffer.
#define BPROP_PRIMITIVE         0x00100000  // Whether the type is a primitive scalar type.
#define BPROP_MIN_PRECISION     0x00200000  // Whether the type is qualified with a minimum precision.
#define BPROP_ROVBUFFER         0x00400000  // Whether the type is a ROV object.
#define BPROP_ENUM              0x00800000  // Whether the type is a enum

#define GET_BPROP_PRIM_KIND(_Props) \
    ((_Props) & (BPROP_BOOLEAN | BPROP_INTEGER | BPROP_FLOATING))

#define GET_BPROP_PRIM_KIND_SU(_Props) \
    ((_Props) & (BPROP_BOOLEAN | BPROP_INTEGER | BPROP_FLOATING | BPROP_UNSIGNED))

#define IS_BPROP_PRIMITIVE(_Props) \
    (((_Props) & BPROP_PRIMITIVE) != 0)

#define IS_BPROP_BOOL(_Props) \
    (((_Props) & BPROP_BOOLEAN) != 0)

#define IS_BPROP_FLOAT(_Props) \
    (((_Props) & BPROP_FLOATING) != 0)

#define IS_BPROP_SINT(_Props) \
    (((_Props) & (BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BOOLEAN)) == \
     BPROP_INTEGER)

#define IS_BPROP_UINT(_Props) \
    (((_Props) & (BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BOOLEAN)) == \
     (BPROP_INTEGER | BPROP_UNSIGNED))

#define IS_BPROP_AINT(_Props) \
    (((_Props) & (BPROP_INTEGER | BPROP_BOOLEAN)) == BPROP_INTEGER)

#define IS_BPROP_STREAM(_Props) \
    (((_Props) & BPROP_STREAM) != 0)

#define IS_BPROP_SAMPLER(_Props) \
    (((_Props) & BPROP_SAMPLER) != 0)

#define IS_BPROP_TEXTURE(_Props) \
    (((_Props) & BPROP_TEXTURE) != 0)

#define IS_BPROP_OBJECT(_Props) \
    (((_Props) & BPROP_OBJECT) != 0)

#define IS_BPROP_MIN_PRECISION(_Props) \
    (((_Props) & BPROP_MIN_PRECISION) != 0)

#define IS_BPROP_UNSIGNABLE(_Props) \
    (IS_BPROP_AINT(_Props) && GET_BPROP_BITS(_Props) != BPROP_BITS12)

#define IS_BPROP_ENUM(_Props) \
    (((_Props) & BPROP_ENUM) != 0)

const UINT g_uBasicKindProps[] =
{
  BPROP_PRIMITIVE | BPROP_BOOLEAN | BPROP_INTEGER | BPROP_NUMERIC | BPROP_BITS0,  // AR_BASIC_BOOL

  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_LITERAL | BPROP_BITS0, // AR_BASIC_LITERAL_FLOAT
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS16,                // AR_BASIC_FLOAT16
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS32 | BPROP_PARTIAL_PRECISION,  // AR_BASIC_FLOAT32_PARTIAL_PRECISION
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS32,                // AR_BASIC_FLOAT32
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS64,                // AR_BASIC_FLOAT64

  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_LITERAL | BPROP_BITS0,  // AR_BASIC_LITERAL_INT
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS8,                  // AR_BASIC_INT8
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BITS8, // AR_BASIC_UINT8
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS16,                 // AR_BASIC_INT16
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BITS16,// AR_BASIC_UINT16
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS32,                 // AR_BASIC_INT32
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BITS32,// AR_BASIC_UINT32
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS64,                 // AR_BASIC_INT64
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BITS64,// AR_BASIC_UINT64

  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS10 | BPROP_MIN_PRECISION,  // AR_BASIC_MIN10FLOAT
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_FLOATING | BPROP_BITS16 | BPROP_MIN_PRECISION,  // AR_BASIC_MIN16FLOAT

  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS12 | BPROP_MIN_PRECISION,   // AR_BASIC_MIN12INT
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_BITS16 | BPROP_MIN_PRECISION,   // AR_BASIC_MIN16INT
  BPROP_PRIMITIVE | BPROP_NUMERIC | BPROP_INTEGER | BPROP_UNSIGNED | BPROP_BITS16 | BPROP_MIN_PRECISION,  // AR_BASIC_MIN16UINT

  BPROP_ENUM | BPROP_NUMERIC | BPROP_INTEGER, // AR_BASIC_ENUM
  BPROP_OTHER,  // AR_BASIC_COUNT

  //
  // Pseudo-entries for intrinsic tables and such.
  //

  0,            // AR_BASIC_NONE
  BPROP_OTHER,  // AR_BASIC_UNKNOWN
  BPROP_OTHER,  // AR_BASIC_NOCAST

  //
  // The following pseudo-entries represent higher-level
  // object types that are treated as units.
  //

  BPROP_POINTER,  // AR_BASIC_POINTER
  BPROP_ENUM, // AR_BASIC_ENUM_CLASS

  BPROP_OBJECT | BPROP_RBUFFER, // AR_OBJECT_NULL
  BPROP_OBJECT | BPROP_RBUFFER,  // AR_OBJECT_STRING_LITERAL
  BPROP_OBJECT | BPROP_RBUFFER, // AR_OBJECT_STRING


  // BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE1D
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE1D_ARRAY
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE2D
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE2D_ARRAY
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE3D
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURECUBE
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURECUBE_ARRAY
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE2DMS
  BPROP_OBJECT | BPROP_TEXTURE, // AR_OBJECT_TEXTURE2DMS_ARRAY

  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLER
  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLER1D
  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLER2D
  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLER3D
  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLERCUBE
  BPROP_OBJECT | BPROP_SAMPLER, // AR_OBJECT_SAMPLERCOMPARISON

  BPROP_OBJECT | BPROP_RBUFFER, // AR_OBJECT_BUFFER
  BPROP_OBJECT,                 // AR_OBJECT_RENDERTARGETVIEW
  BPROP_OBJECT,                 // AR_OBJECT_DEPTHSTENCILVIEW

  BPROP_OBJECT,   // AR_OBJECT_COMPUTESHADER
  BPROP_OBJECT,   // AR_OBJECT_DOMAINSHADER
  BPROP_OBJECT,   // AR_OBJECT_GEOMETRYSHADER
  BPROP_OBJECT,   // AR_OBJECT_HULLSHADER
  BPROP_OBJECT,   // AR_OBJECT_PIXELSHADER
  BPROP_OBJECT,   // AR_OBJECT_VERTEXSHADER
  BPROP_OBJECT,   // AR_OBJECT_PIXELFRAGMENT
  BPROP_OBJECT,   // AR_OBJECT_VERTEXFRAGMENT

  BPROP_OBJECT,   // AR_OBJECT_STATEBLOCK

  BPROP_OBJECT,   // AR_OBJECT_RASTERIZER
  BPROP_OBJECT,   // AR_OBJECT_DEPTHSTENCIL
  BPROP_OBJECT,   // AR_OBJECT_BLEND

  BPROP_OBJECT | BPROP_STREAM,    // AR_OBJECT_POINTSTREAM
  BPROP_OBJECT | BPROP_STREAM,    // AR_OBJECT_LINESTREAM
  BPROP_OBJECT | BPROP_STREAM,    // AR_OBJECT_TRIANGLESTREAM

  BPROP_OBJECT | BPROP_PATCH,     // AR_OBJECT_INPUTPATCH
  BPROP_OBJECT | BPROP_PATCH,     // AR_OBJECT_OUTPUTPATCH

  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWTEXTURE1D
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWTEXTURE1D_ARRAY
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWTEXTURE2D
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWTEXTURE2D_ARRAY
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWTEXTURE3D
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWBUFFER

  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_BYTEADDRESS_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWBYTEADDRESS_BUFFER
  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_STRUCTURED_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWSTRUCTURED_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_APPEND_STRUCTURED_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER,  // AR_OBJECT_CONSUME_STRUCTURED_BUFFER

  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_CONSTANT_BUFFER
  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_TEXTURE_BUFFER

  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVBUFFER
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVBYTEADDRESS_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVSTRUCTURED_BUFFER
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVTEXTURE1D
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVTEXTURE1D_ARRAY
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVTEXTURE2D
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVTEXTURE2D_ARRAY
  BPROP_OBJECT | BPROP_RWBUFFER | BPROP_ROVBUFFER,    // AR_OBJECT_ROVTEXTURE3D

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_VK_SUBPASS_INPUT
  BPROP_OBJECT | BPROP_RBUFFER,   // AR_OBJECT_VK_SUBPASS_INPUT_MS
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  BPROP_OBJECT,   // AR_OBJECT_INNER

  BPROP_OBJECT,   // AR_OBJECT_LEGACY_EFFECT

  BPROP_OBJECT,   // AR_OBJECT_WAVE

  LICOMPTYPE_RAYDESC,               // AR_OBJECT_RAY_DESC
  LICOMPTYPE_ACCELERATION_STRUCT,   // AR_OBJECT_ACCELERATION_STRUCT
  LICOMPTYPE_USER_DEFINED_TYPE,      // AR_OBJECT_USER_DEFINED_TYPE
  0,      // AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES

  // subobjects
  0,      //AR_OBJECT_STATE_OBJECT_CONFIG,
  0,      //AR_OBJECT_GLOBAL_ROOT_SIGNATURE,
  0,      //AR_OBJECT_LOCAL_ROOT_SIGNATURE,
  0,      //AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC,
  0,      //AR_OBJECT_RAYTRACING_SHADER_CONFIG,
  0,      //AR_OBJECT_RAYTRACING_PIPELINE_CONFIG,
  0,      //AR_OBJECT_TRIANGLE_HIT_GROUP,
  0,      //AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP,

  // AR_BASIC_MAXIMUM_COUNT
};

C_ASSERT(ARRAYSIZE(g_uBasicKindProps) == AR_BASIC_MAXIMUM_COUNT);

#define GetBasicKindProps(_Kind) g_uBasicKindProps[(_Kind)]

#define GET_BASIC_BITS(_Kind) \
    GET_BPROP_BITS(GetBasicKindProps(_Kind))

#define GET_BASIC_PRIM_KIND(_Kind) \
    GET_BPROP_PRIM_KIND(GetBasicKindProps(_Kind))
#define GET_BASIC_PRIM_KIND_SU(_Kind) \
    GET_BPROP_PRIM_KIND_SU(GetBasicKindProps(_Kind))

#define IS_BASIC_PRIMITIVE(_Kind) \
    IS_BPROP_PRIMITIVE(GetBasicKindProps(_Kind))

#define IS_BASIC_BOOL(_Kind) \
    IS_BPROP_BOOL(GetBasicKindProps(_Kind))

#define IS_BASIC_FLOAT(_Kind) \
    IS_BPROP_FLOAT(GetBasicKindProps(_Kind))

#define IS_BASIC_SINT(_Kind) \
    IS_BPROP_SINT(GetBasicKindProps(_Kind))
#define IS_BASIC_UINT(_Kind) \
    IS_BPROP_UINT(GetBasicKindProps(_Kind))
#define IS_BASIC_AINT(_Kind) \
    IS_BPROP_AINT(GetBasicKindProps(_Kind))

#define IS_BASIC_STREAM(_Kind) \
    IS_BPROP_STREAM(GetBasicKindProps(_Kind))

#define IS_BASIC_SAMPLER(_Kind) \
    IS_BPROP_SAMPLER(GetBasicKindProps(_Kind))
#define IS_BASIC_TEXTURE(_Kind) \
    IS_BPROP_TEXTURE(GetBasicKindProps(_Kind))
#define IS_BASIC_OBJECT(_Kind) \
    IS_BPROP_OBJECT(GetBasicKindProps(_Kind))

#define IS_BASIC_MIN_PRECISION(_Kind) \
    IS_BPROP_MIN_PRECISION(GetBasicKindProps(_Kind))

#define IS_BASIC_UNSIGNABLE(_Kind) \
    IS_BPROP_UNSIGNABLE(GetBasicKindProps(_Kind))

#define IS_BASIC_ENUM(_Kind) \
    IS_BPROP_ENUM(GetBasicKindProps(_Kind))

#define BITWISE_ENUM_OPS(_Type)                                         \
inline _Type operator|(_Type F1, _Type F2)                              \
{                                                                       \
    return (_Type)((UINT)F1 | (UINT)F2);                                \
}                                                                       \
inline _Type operator&(_Type F1, _Type F2)                              \
{                                                                       \
    return (_Type)((UINT)F1 & (UINT)F2);                                \
}                                                                       \
inline _Type& operator|=(_Type& F1, _Type F2)                           \
{                                                                       \
    F1 = F1 | F2;                                                       \
    return F1;                                                          \
}                                                                       \
inline _Type& operator&=(_Type& F1, _Type F2)                           \
{                                                                       \
    F1 = F1 & F2;                                                       \
    return F1;                                                          \
}                                                                       \
inline _Type& operator&=(_Type& F1, UINT F2)                            \
{                                                                       \
    F1 = (_Type)((UINT)F1 & F2);                                        \
    return F1;                                                          \
}

enum ArTypeObjectKind {
  AR_TOBJ_INVALID,   // Flag for an unassigned / unavailable object type.
  AR_TOBJ_VOID,      // Represents the type for functions with not returned valued.
  AR_TOBJ_BASIC,     // Represents a primitive type.
  AR_TOBJ_COMPOUND,  // Represents a struct or class.
  AR_TOBJ_INTERFACE, // Represents an interface.
  AR_TOBJ_POINTER,   // Represents a pointer to another type.
  AR_TOBJ_OBJECT,    // Represents a built-in object.
  AR_TOBJ_ARRAY,     // Represents an array of other types.
  AR_TOBJ_MATRIX,    // Represents a matrix of basic types.
  AR_TOBJ_VECTOR,    // Represents a vector of basic types.
  AR_TOBJ_QUALIFIER, // Represents another type plus an ArTypeQualifier.
  AR_TOBJ_INNER_OBJ, // Represents a built-in inner object, such as an 
                     // indexer object used to implement .mips[1].
  AR_TOBJ_STRING,    // Represents a string
};

enum TYPE_CONVERSION_FLAGS
{
  TYPE_CONVERSION_DEFAULT = 0x00000000, // Indicates an implicit conversion is done.
  TYPE_CONVERSION_EXPLICIT = 0x00000001, // Indicates a conversion is done through an explicit cast.
  TYPE_CONVERSION_BY_REFERENCE = 0x00000002, // Indicates a conversion is done to an output parameter.
};

enum TYPE_CONVERSION_REMARKS
{
  TYPE_CONVERSION_NONE = 0x00000000,
  TYPE_CONVERSION_PRECISION_LOSS = 0x00000001,
  TYPE_CONVERSION_IDENTICAL = 0x00000002,
  TYPE_CONVERSION_TO_VOID = 0x00000004,
  TYPE_CONVERSION_ELT_TRUNCATION = 0x00000008,
};

BITWISE_ENUM_OPS(TYPE_CONVERSION_REMARKS)

#define AR_TOBJ_SCALAR AR_TOBJ_BASIC
#define AR_TOBJ_UNKNOWN AR_TOBJ_INVALID

#define AR_TPROP_VOID              0x0000000000000001
#define AR_TPROP_CONST             0x0000000000000002
#define AR_TPROP_IMP_CONST         0x0000000000000004
#define AR_TPROP_OBJECT            0x0000000000000008
#define AR_TPROP_SCALAR            0x0000000000000010
#define AR_TPROP_UNSIGNED          0x0000000000000020
#define AR_TPROP_NUMERIC           0x0000000000000040
#define AR_TPROP_INTEGRAL          0x0000000000000080
#define AR_TPROP_FLOATING          0x0000000000000100
#define AR_TPROP_LITERAL           0x0000000000000200
#define AR_TPROP_POINTER           0x0000000000000400
#define AR_TPROP_INPUT_PATCH       0x0000000000000800
#define AR_TPROP_OUTPUT_PATCH      0x0000000000001000
#define AR_TPROP_INH_IFACE         0x0000000000002000
#define AR_TPROP_HAS_COMPOUND      0x0000000000004000
#define AR_TPROP_HAS_TEXTURES      0x0000000000008000
#define AR_TPROP_HAS_SAMPLERS      0x0000000000010000
#define AR_TPROP_HAS_SAMPLER_CMPS  0x0000000000020000
#define AR_TPROP_HAS_STREAMS       0x0000000000040000
#define AR_TPROP_HAS_OTHER_OBJECTS 0x0000000000080000
#define AR_TPROP_HAS_BASIC         0x0000000000100000
#define AR_TPROP_HAS_BUFFERS       0x0000000000200000
#define AR_TPROP_HAS_ROBJECTS      0x0000000000400000
#define AR_TPROP_HAS_POINTERS      0x0000000000800000
#define AR_TPROP_INDEXABLE         0x0000000001000000
#define AR_TPROP_HAS_MIPS          0x0000000002000000
#define AR_TPROP_WRITABLE_GLOBAL   0x0000000004000000
#define AR_TPROP_HAS_UAVS          0x0000000008000000
#define AR_TPROP_HAS_BYTEADDRESS   0x0000000010000000
#define AR_TPROP_HAS_STRUCTURED    0x0000000020000000
#define AR_TPROP_HAS_SAMPLE        0x0000000040000000
#define AR_TPROP_MIN_PRECISION     0x0000000080000000
#define AR_TPROP_HAS_CBUFFERS      0x0000000100008000
#define AR_TPROP_HAS_TBUFFERS      0x0000000200008000

#define AR_TPROP_ALL               0xffffffffffffffff

#define AR_TPROP_HAS_OBJECTS \
    (AR_TPROP_HAS_TEXTURES | AR_TPROP_HAS_SAMPLERS | \
     AR_TPROP_HAS_SAMPLER_CMPS | AR_TPROP_HAS_STREAMS | \
     AR_TPROP_HAS_OTHER_OBJECTS | AR_TPROP_HAS_BUFFERS | \
     AR_TPROP_HAS_ROBJECTS | AR_TPROP_HAS_UAVS | \
     AR_TPROP_HAS_BYTEADDRESS | AR_TPROP_HAS_STRUCTURED)

#define AR_TPROP_HAS_BASIC_RESOURCES \
    (AR_TPROP_HAS_TEXTURES | AR_TPROP_HAS_SAMPLERS | \
     AR_TPROP_HAS_SAMPLER_CMPS | AR_TPROP_HAS_BUFFERS | \
     AR_TPROP_HAS_UAVS)

#define AR_TPROP_UNION_BITS \
    (AR_TPROP_INH_IFACE | AR_TPROP_HAS_COMPOUND | AR_TPROP_HAS_TEXTURES | \
     AR_TPROP_HAS_SAMPLERS | AR_TPROP_HAS_SAMPLER_CMPS | \
     AR_TPROP_HAS_STREAMS | AR_TPROP_HAS_OTHER_OBJECTS | AR_TPROP_HAS_BASIC | \
     AR_TPROP_HAS_BUFFERS | AR_TPROP_HAS_ROBJECTS | AR_TPROP_HAS_POINTERS | \
     AR_TPROP_WRITABLE_GLOBAL | AR_TPROP_HAS_UAVS | \
     AR_TPROP_HAS_BYTEADDRESS | AR_TPROP_HAS_STRUCTURED | AR_TPROP_MIN_PRECISION)

#define AR_TINFO_ALLOW_COMPLEX       0x00000001
#define AR_TINFO_ALLOW_OBJECTS       0x00000002
#define AR_TINFO_IGNORE_QUALIFIERS   0x00000004
#define AR_TINFO_OBJECTS_AS_ELEMENTS 0x00000008
#define AR_TINFO_PACK_SCALAR         0x00000010
#define AR_TINFO_PACK_ROW_MAJOR      0x00000020
#define AR_TINFO_PACK_TEMP_ARRAY     0x00000040
#define AR_TINFO_ALL_VAR_INFO        0x00000080

#define AR_TINFO_ALLOW_ALL (AR_TINFO_ALLOW_COMPLEX | AR_TINFO_ALLOW_OBJECTS)

#define AR_TINFO_PACK_CBUFFER 0
#define AR_TINFO_LAYOUT_PACK_ALL (AR_TINFO_PACK_SCALAR | AR_TINFO_PACK_TEMP_ARRAY)

#define AR_TINFO_SIMPLE_OBJECTS \
    (AR_TINFO_ALLOW_OBJECTS | AR_TINFO_OBJECTS_AS_ELEMENTS)

struct ArTypeInfo {
  ArTypeObjectKind ShapeKind;      // The shape of the type (basic, matrix, etc.)
  ArBasicKind EltKind;             // The primitive type of elements in this type.
  ArBasicKind ObjKind;             // The object type for this type (textures, buffers, etc.)
  UINT uRows;
  UINT uCols;
  UINT uTotalElts;
};

using namespace clang;
using namespace clang::sema;
using namespace hlsl;

extern const char *HLSLScalarTypeNames[];

static const bool ExplicitConversionFalse = false;// a conversion operation is not the result of an explicit cast
static const bool ParameterPackFalse = false;     // template parameter is not an ellipsis.
static const bool TypenameTrue = false;           // 'typename' specified rather than 'class' for a template argument.
static const bool DelayTypeCreationTrue = true;   // delay type creation for a declaration
static const SourceLocation NoLoc;                // no source location attribution available
static const SourceRange NoRange;                 // no source range attribution available
static const bool HasWrittenPrototypeTrue = true; // function had the prototype written
static const bool InlineSpecifiedFalse = false;   // function was not specified as inline
static const bool IsConstexprFalse = false;       // function is not constexpr
static const bool ListInitializationFalse = false;// not performing a list initialization
static const bool SuppressWarningsFalse = false;  // do not suppress warning diagnostics
static const bool SuppressErrorsTrue = true;      // suppress error diagnostics
static const bool SuppressErrorsFalse = false;    // do not suppress error diagnostics
static const int OneRow = 1;                      // a single row for a type
static const bool MipsFalse = false;              // a type does not support the .mips member
static const bool MipsTrue = true;                // a type supports the .mips member
static const bool SampleFalse = false;            // a type does not support the .sample member
static const bool SampleTrue = true;              // a type supports the .sample member
static const size_t MaxVectorSize = 4;            // maximum size for a vector

static 
QualType GetOrCreateTemplateSpecialization(
  ASTContext& context, 
  Sema& sema,
  _In_ ClassTemplateDecl* templateDecl,
  ArrayRef<TemplateArgument> templateArgs
  )
{
  DXASSERT_NOMSG(templateDecl);
  DeclContext* currentDeclContext = context.getTranslationUnitDecl();
  SmallVector<TemplateArgument, 3> templateArgsForDecl;
  for (const TemplateArgument& Arg : templateArgs) {
    if (Arg.getKind() == TemplateArgument::Type) {
        // the class template need to use CanonicalType
        templateArgsForDecl.emplace_back(TemplateArgument(Arg.getAsType().getCanonicalType()));
    }else
        templateArgsForDecl.emplace_back(Arg);
  }
  // First, try looking up existing specialization
  void* InsertPos = nullptr;
  ClassTemplateSpecializationDecl* specializationDecl = 
    templateDecl->findSpecialization(templateArgsForDecl, InsertPos);
  if (specializationDecl) {
    // Instantiate the class template if not yet.
    if (specializationDecl->getInstantiatedFrom().isNull()) {
      // InstantiateClassTemplateSpecialization returns true if it finds an
      // error.
      DXVERIFY_NOMSG(false ==
                     sema.InstantiateClassTemplateSpecialization(
                         NoLoc, specializationDecl,
                         TemplateSpecializationKind::TSK_ImplicitInstantiation,
                         true));
    }
    return context.getTemplateSpecializationType(
        TemplateName(templateDecl), templateArgs.data(), templateArgs.size(),
        context.getTypeDeclType(specializationDecl));
  }

  specializationDecl = ClassTemplateSpecializationDecl::Create(
    context, TagDecl::TagKind::TTK_Class, currentDeclContext, NoLoc, NoLoc,
    templateDecl, templateArgsForDecl.data(), templateArgsForDecl.size(), nullptr);
  // InstantiateClassTemplateSpecialization returns true if it finds an error.
  DXVERIFY_NOMSG(false == sema.InstantiateClassTemplateSpecialization(
    NoLoc, specializationDecl, TemplateSpecializationKind::TSK_ImplicitInstantiation, true));
  templateDecl->AddSpecialization(specializationDecl, InsertPos);
  specializationDecl->setImplicit(true);

  QualType canonType = context.getTypeDeclType(specializationDecl);
  DXASSERT(isa<RecordType>(canonType), "type of non-dependent specialization is not a RecordType");
  TemplateArgumentListInfo templateArgumentList(NoLoc, NoLoc);
  TemplateArgumentLocInfo NoTemplateArgumentLocInfo;
  for (unsigned i = 0; i < templateArgs.size(); i++) {
    templateArgumentList.addArgument(TemplateArgumentLoc(templateArgs[i], NoTemplateArgumentLocInfo));
  }
  return context.getTemplateSpecializationType(
    TemplateName(templateDecl), templateArgumentList, canonType);
}

/// <summary>Instantiates a new matrix type specialization or gets an existing one from the AST.</summary>
static
QualType GetOrCreateMatrixSpecialization(ASTContext& context, Sema* sema,
  _In_ ClassTemplateDecl* matrixTemplateDecl,
  QualType elementType, uint64_t rowCount, uint64_t colCount)
{
  DXASSERT_NOMSG(sema);

  TemplateArgument templateArgs[3] = {
      TemplateArgument(elementType),
      TemplateArgument(
          context,
          llvm::APSInt(
              llvm::APInt(context.getIntWidth(context.IntTy), rowCount), false),
          context.IntTy),
      TemplateArgument(
          context,
          llvm::APSInt(
              llvm::APInt(context.getIntWidth(context.IntTy), colCount), false),
          context.IntTy)};

  QualType matrixSpecializationType = GetOrCreateTemplateSpecialization(context, *sema, matrixTemplateDecl, ArrayRef<TemplateArgument>(templateArgs));

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DXASSERT(matrixSpecializationType->getAsCXXRecordDecl(), 
           "type of non-dependent specialization is not a RecordType");
  DeclContext::lookup_result lookupResult = matrixSpecializationType->getAsCXXRecordDecl()->
    lookup(DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise matrix handle cannot be looked up");
#endif

  return matrixSpecializationType;
}

/// <summary>Instantiates a new vector type specialization or gets an existing one from the AST.</summary>
static
QualType GetOrCreateVectorSpecialization(ASTContext& context, Sema* sema,
  _In_ ClassTemplateDecl* vectorTemplateDecl,
  QualType elementType, uint64_t colCount)
{
  DXASSERT_NOMSG(sema);
  DXASSERT_NOMSG(vectorTemplateDecl);

  TemplateArgument templateArgs[2] = {
      TemplateArgument(elementType),
      TemplateArgument(
          context,
          llvm::APSInt(
              llvm::APInt(context.getIntWidth(context.IntTy), colCount), false),
          context.IntTy)};

  QualType vectorSpecializationType = GetOrCreateTemplateSpecialization(context, *sema, vectorTemplateDecl, ArrayRef<TemplateArgument>(templateArgs));

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DXASSERT(vectorSpecializationType->getAsCXXRecordDecl(), 
           "type of non-dependent specialization is not a RecordType");
  DeclContext::lookup_result lookupResult = vectorSpecializationType->getAsCXXRecordDecl()->
    lookup(DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise vector handle cannot be looked up");
#endif

  return vectorSpecializationType;
}


// Decls.cpp constants start here - these should be refactored or, better, replaced with clang::Type-based constructs.

static const LPCSTR kBuiltinIntrinsicTableName = "op";

static const unsigned kAtomicDstOperandIdx = 1;

static const ArTypeObjectKind g_ScalarTT[] =
{
  AR_TOBJ_SCALAR,
  AR_TOBJ_UNKNOWN
};

static const ArTypeObjectKind g_VectorTT[] =
{
  AR_TOBJ_VECTOR,
  AR_TOBJ_UNKNOWN
};

static const ArTypeObjectKind g_MatrixTT[] =
{
  AR_TOBJ_MATRIX,
  AR_TOBJ_UNKNOWN
};

static const ArTypeObjectKind g_AnyTT[] =
{
  AR_TOBJ_SCALAR,
  AR_TOBJ_VECTOR,
  AR_TOBJ_MATRIX,
  AR_TOBJ_UNKNOWN
};

static const ArTypeObjectKind g_ObjectTT[] =
{
  AR_TOBJ_OBJECT,
  AR_TOBJ_UNKNOWN
};

static const ArTypeObjectKind g_NullTT[] =
{
  AR_TOBJ_VOID,
  AR_TOBJ_UNKNOWN
};

const ArTypeObjectKind* g_LegalIntrinsicTemplates[] =
{
  g_NullTT,
  g_ScalarTT,
  g_VectorTT,
  g_MatrixTT,
  g_AnyTT,
  g_ObjectTT,
};
C_ASSERT(ARRAYSIZE(g_LegalIntrinsicTemplates) == LITEMPLATE_COUNT);

//
// The first one is used to name the representative group, so make
// sure its name will make sense in error messages.
//

static const ArBasicKind g_BoolCT[] =
{
  AR_BASIC_BOOL,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_IntCT[] =
{
  AR_BASIC_INT32,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_UIntCT[] =
{
  AR_BASIC_UINT32,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

// We use the first element for default if matching kind is missing in the list.
// AR_BASIC_INT32 should be the default for any int since min precision integers should map to int32, not int16 or int64
static const ArBasicKind g_AnyIntCT[] =
{
  AR_BASIC_INT32,
  AR_BASIC_INT16,
  AR_BASIC_UINT32,
  AR_BASIC_UINT16,
  AR_BASIC_INT64,
  AR_BASIC_UINT64,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_AnyInt32CT[] =
{
  AR_BASIC_INT32,
  AR_BASIC_UINT32,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_UIntOnlyCT[] =
{
  AR_BASIC_UINT32,
  AR_BASIC_UINT64,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_NOCAST,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_FloatCT[] =
{
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_AnyFloatCT[] =
{
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT16,
  AR_BASIC_FLOAT64,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_MIN10FLOAT,
  AR_BASIC_MIN16FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_FloatLikeCT[] =
{
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT16,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_MIN10FLOAT,
  AR_BASIC_MIN16FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_FloatDoubleCT[] =
{
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT64,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_DoubleCT[] =
{
  AR_BASIC_FLOAT64,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_DoubleOnlyCT[] =
{
  AR_BASIC_FLOAT64,
  AR_BASIC_NOCAST,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_NumericCT[] =
{
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT16,
  AR_BASIC_FLOAT64,
  AR_BASIC_MIN10FLOAT,
  AR_BASIC_MIN16FLOAT,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_INT16,
  AR_BASIC_INT32,
  AR_BASIC_UINT16,
  AR_BASIC_UINT32,
  AR_BASIC_MIN12INT,
  AR_BASIC_MIN16INT,
  AR_BASIC_MIN16UINT,
  AR_BASIC_INT64,
  AR_BASIC_UINT64,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Numeric32CT[] =
{
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_INT32,
  AR_BASIC_UINT32,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Numeric32OnlyCT[] =
{
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_INT32,
  AR_BASIC_UINT32,
  AR_BASIC_NOCAST,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_AnyCT[] =
{
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_FLOAT32,
  AR_BASIC_FLOAT32_PARTIAL_PRECISION,
  AR_BASIC_FLOAT16,
  AR_BASIC_FLOAT64,
  AR_BASIC_MIN10FLOAT,
  AR_BASIC_MIN16FLOAT,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_INT16,
  AR_BASIC_UINT16,
  AR_BASIC_INT32,
  AR_BASIC_UINT32,
  AR_BASIC_MIN12INT,
  AR_BASIC_MIN16INT,
  AR_BASIC_MIN16UINT,
  AR_BASIC_BOOL,
  AR_BASIC_INT64,
  AR_BASIC_UINT64,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Sampler1DCT[] =
{
  AR_OBJECT_SAMPLER1D,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Sampler2DCT[] =
{
  AR_OBJECT_SAMPLER2D,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Sampler3DCT[] =
{
  AR_OBJECT_SAMPLER3D,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_SamplerCUBECT[] =
{
  AR_OBJECT_SAMPLERCUBE,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_SamplerCmpCT[] =
{
  AR_OBJECT_SAMPLERCOMPARISON,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_SamplerCT[] =
{
  AR_OBJECT_SAMPLER,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_RayDescCT[] =
{
  AR_OBJECT_RAY_DESC,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_AccelerationStructCT[] =
{
  AR_OBJECT_ACCELERATION_STRUCT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_UDTCT[] =
{
  AR_OBJECT_USER_DEFINED_TYPE,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_StringCT[] =
{
  AR_OBJECT_STRING_LITERAL,
  AR_OBJECT_STRING,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_NullCT[] =
{
  AR_OBJECT_NULL,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_WaveCT[] =
{
  AR_OBJECT_WAVE,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_UInt64CT[] =
{
  AR_BASIC_UINT64,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Float16CT[] =
{
  AR_BASIC_FLOAT16,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Int16CT[] =
{
  AR_BASIC_INT16,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_UInt16CT[] =
{
  AR_BASIC_UINT16,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_UNKNOWN
};

static const ArBasicKind g_Numeric16OnlyCT[] =
{
  AR_BASIC_FLOAT16,
  AR_BASIC_INT16,
  AR_BASIC_UINT16,
  AR_BASIC_LITERAL_FLOAT,
  AR_BASIC_LITERAL_INT,
  AR_BASIC_NOCAST,
  AR_BASIC_UNKNOWN
};

// Basic kinds, indexed by a LEGAL_INTRINSIC_COMPTYPES value.
const ArBasicKind* g_LegalIntrinsicCompTypes[] =
{
  g_NullCT,             // LICOMPTYPE_VOID
  g_BoolCT,             // LICOMPTYPE_BOOL
  g_IntCT,              // LICOMPTYPE_INT
  g_UIntCT,             // LICOMPTYPE_UINT
  g_AnyIntCT,           // LICOMPTYPE_ANY_INT
  g_AnyInt32CT,         // LICOMPTYPE_ANY_INT32
  g_UIntOnlyCT,         // LICOMPTYPE_UINT_ONLY
  g_FloatCT,            // LICOMPTYPE_FLOAT
  g_AnyFloatCT,         // LICOMPTYPE_ANY_FLOAT
  g_FloatLikeCT,        // LICOMPTYPE_FLOAT_LIKE
  g_FloatDoubleCT,      // LICOMPTYPE_FLOAT_DOUBLE
  g_DoubleCT,           // LICOMPTYPE_DOUBLE
  g_DoubleOnlyCT,       // LICOMPTYPE_DOUBLE_ONLY
  g_NumericCT,          // LICOMPTYPE_NUMERIC
  g_Numeric32CT,        // LICOMPTYPE_NUMERIC32
  g_Numeric32OnlyCT,    // LICOMPTYPE_NUMERIC32_ONLY
  g_AnyCT,              // LICOMPTYPE_ANY
  g_Sampler1DCT,        // LICOMPTYPE_SAMPLER1D
  g_Sampler2DCT,        // LICOMPTYPE_SAMPLER2D
  g_Sampler3DCT,        // LICOMPTYPE_SAMPLER3D
  g_SamplerCUBECT,      // LICOMPTYPE_SAMPLERCUBE
  g_SamplerCmpCT,       // LICOMPTYPE_SAMPLERCMP
  g_SamplerCT,          // LICOMPTYPE_SAMPLER
  g_StringCT,           // LICOMPTYPE_STRING
  g_WaveCT,             // LICOMPTYPE_WAVE
  g_UInt64CT,           // LICOMPTYPE_UINT64
  g_Float16CT,          // LICOMPTYPE_FLOAT16
  g_Int16CT,            // LICOMPTYPE_INT16
  g_UInt16CT,           // LICOMPTYPE_UINT16
  g_Numeric16OnlyCT,    // LICOMPTYPE_NUMERIC16_ONLY
  g_RayDescCT,          // LICOMPTYPE_RAYDESC
  g_AccelerationStructCT,   // LICOMPTYPE_ACCELERATION_STRUCT,
  g_UDTCT,              // LICOMPTYPE_USER_DEFINED_TYPE
};
C_ASSERT(ARRAYSIZE(g_LegalIntrinsicCompTypes) == LICOMPTYPE_COUNT);

// Decls.cpp constants ends here - these should be refactored or, better, replaced with clang::Type-based constructs.

// Basic kind objects that are represented as HLSL structures or templates.
static
const ArBasicKind g_ArBasicKindsAsTypes[] =
{
  AR_OBJECT_BUFFER,             // Buffer

  // AR_OBJECT_TEXTURE,
  AR_OBJECT_TEXTURE1D,          // Texture1D
  AR_OBJECT_TEXTURE1D_ARRAY,    // Texture1DArray
  AR_OBJECT_TEXTURE2D,          // Texture2D
  AR_OBJECT_TEXTURE2D_ARRAY,    // Texture2DArray
  AR_OBJECT_TEXTURE3D,          // Texture3D
  AR_OBJECT_TEXTURECUBE,        // TextureCube
  AR_OBJECT_TEXTURECUBE_ARRAY,  // TextureCubeArray
  AR_OBJECT_TEXTURE2DMS,        // Texture2DMS
  AR_OBJECT_TEXTURE2DMS_ARRAY,  // Texture2DMSArray

  AR_OBJECT_SAMPLER,
  //AR_OBJECT_SAMPLER1D,
  //AR_OBJECT_SAMPLER2D,
  //AR_OBJECT_SAMPLER3D,
  //AR_OBJECT_SAMPLERCUBE,
  AR_OBJECT_SAMPLERCOMPARISON,

  AR_OBJECT_POINTSTREAM,
  AR_OBJECT_LINESTREAM,
  AR_OBJECT_TRIANGLESTREAM,

  AR_OBJECT_INPUTPATCH,
  AR_OBJECT_OUTPUTPATCH,

  AR_OBJECT_RWTEXTURE1D,
  AR_OBJECT_RWTEXTURE1D_ARRAY,
  AR_OBJECT_RWTEXTURE2D,
  AR_OBJECT_RWTEXTURE2D_ARRAY,
  AR_OBJECT_RWTEXTURE3D,
  AR_OBJECT_RWBUFFER,

  AR_OBJECT_BYTEADDRESS_BUFFER,
  AR_OBJECT_RWBYTEADDRESS_BUFFER,
  AR_OBJECT_STRUCTURED_BUFFER,
  AR_OBJECT_RWSTRUCTURED_BUFFER,
  // AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC,
  // AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME,
  AR_OBJECT_APPEND_STRUCTURED_BUFFER,
  AR_OBJECT_CONSUME_STRUCTURED_BUFFER,

  AR_OBJECT_ROVBUFFER,
  AR_OBJECT_ROVBYTEADDRESS_BUFFER,
  AR_OBJECT_ROVSTRUCTURED_BUFFER,
  AR_OBJECT_ROVTEXTURE1D,
  AR_OBJECT_ROVTEXTURE1D_ARRAY,
  AR_OBJECT_ROVTEXTURE2D,
  AR_OBJECT_ROVTEXTURE2D_ARRAY,
  AR_OBJECT_ROVTEXTURE3D,

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  AR_OBJECT_VK_SUBPASS_INPUT,
  AR_OBJECT_VK_SUBPASS_INPUT_MS,
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  AR_OBJECT_LEGACY_EFFECT,      // Used for all unsupported but ignored legacy effect types

  AR_OBJECT_WAVE,
  AR_OBJECT_RAY_DESC,
  AR_OBJECT_ACCELERATION_STRUCT,
  AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES,

  // subobjects
  AR_OBJECT_STATE_OBJECT_CONFIG,
  AR_OBJECT_GLOBAL_ROOT_SIGNATURE,
  AR_OBJECT_LOCAL_ROOT_SIGNATURE,
  AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC,
  AR_OBJECT_RAYTRACING_SHADER_CONFIG,
  AR_OBJECT_RAYTRACING_PIPELINE_CONFIG,
  AR_OBJECT_TRIANGLE_HIT_GROUP,
  AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP
};

// Count of template arguments for basic kind of objects that look like templates (one or more type arguments).
static
const uint8_t g_ArBasicKindsTemplateCount[] =
{
  1,  // AR_OBJECT_BUFFER

  // AR_OBJECT_TEXTURE,
  1, // AR_OBJECT_TEXTURE1D
  1, // AR_OBJECT_TEXTURE1D_ARRAY
  1, // AR_OBJECT_TEXTURE2D
  1, // AR_OBJECT_TEXTURE2D_ARRAY
  1, // AR_OBJECT_TEXTURE3D
  1, // AR_OBJECT_TEXTURECUBE
  1, // AR_OBJECT_TEXTURECUBE_ARRAY
  2, // AR_OBJECT_TEXTURE2DMS
  2, // AR_OBJECT_TEXTURE2DMS_ARRAY

  0, // AR_OBJECT_SAMPLER
  //AR_OBJECT_SAMPLER1D,
  //AR_OBJECT_SAMPLER2D,
  //AR_OBJECT_SAMPLER3D,
  //AR_OBJECT_SAMPLERCUBE,
  0, // AR_OBJECT_SAMPLERCOMPARISON

  1, // AR_OBJECT_POINTSTREAM
  1, // AR_OBJECT_LINESTREAM
  1, // AR_OBJECT_TRIANGLESTREAM

  2, // AR_OBJECT_INPUTPATCH
  2, // AR_OBJECT_OUTPUTPATCH

  1, // AR_OBJECT_RWTEXTURE1D
  1, // AR_OBJECT_RWTEXTURE1D_ARRAY
  1, // AR_OBJECT_RWTEXTURE2D
  1, // AR_OBJECT_RWTEXTURE2D_ARRAY
  1, // AR_OBJECT_RWTEXTURE3D
  1, // AR_OBJECT_RWBUFFER

  0, // AR_OBJECT_BYTEADDRESS_BUFFER
  0, // AR_OBJECT_RWBYTEADDRESS_BUFFER
  1, // AR_OBJECT_STRUCTURED_BUFFER
  1, // AR_OBJECT_RWSTRUCTURED_BUFFER
  // 1, // AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC
  // 1, // AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME
  1, // AR_OBJECT_APPEND_STRUCTURED_BUFFER
  1, // AR_OBJECT_CONSUME_STRUCTURED_BUFFER

  1, // AR_OBJECT_ROVBUFFER
  0, // AR_OBJECT_ROVBYTEADDRESS_BUFFER
  1, // AR_OBJECT_ROVSTRUCTURED_BUFFER
  1, // AR_OBJECT_ROVTEXTURE1D
  1, // AR_OBJECT_ROVTEXTURE1D_ARRAY
  1, // AR_OBJECT_ROVTEXTURE2D
  1, // AR_OBJECT_ROVTEXTURE2D_ARRAY
  1, // AR_OBJECT_ROVTEXTURE3D

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  1, // AR_OBJECT_VK_SUBPASS_INPUT
  1, // AR_OBJECT_VK_SUBPASS_INPUT_MS
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  0, // AR_OBJECT_LEGACY_EFFECT   // Used for all unsupported but ignored legacy effect types
  0, // AR_OBJECT_WAVE
  0, // AR_OBJECT_RAY_DESC
  0, // AR_OBJECT_ACCELERATION_STRUCT
  0, // AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES

  0, // AR_OBJECT_STATE_OBJECT_CONFIG,
  0, // AR_OBJECT_GLOBAL_ROOT_SIGNATURE,
  0, // AR_OBJECT_LOCAL_ROOT_SIGNATURE,
  0, // AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC,
  0, // AR_OBJECT_RAYTRACING_SHADER_CONFIG,
  0, // AR_OBJECT_RAYTRACING_PIPELINE_CONFIG,
  0, // AR_OBJECT_TRIANGLE_HIT_GROUP,
  0, // AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP,
};

C_ASSERT(_countof(g_ArBasicKindsAsTypes) == _countof(g_ArBasicKindsTemplateCount));

/// <summary>Describes the how the subscript or indexing operators work on a given type.</summary>
struct SubscriptOperatorRecord
{
  unsigned int SubscriptCardinality : 4;  // Number of elements expected in subscript - zero if operator not supported.
  bool HasMips : 1;                       // true if the kind has a mips member; false otherwise
  bool HasSample : 1;                     // true if the kind has a sample member; false otherwise
};

// Subscript operators for objects that are represented as HLSL structures or templates.
static
const SubscriptOperatorRecord g_ArBasicKindsSubscripts[] =
{
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_BUFFER (Buffer)

  // AR_OBJECT_TEXTURE,
  { 1, MipsTrue,  SampleFalse }, // AR_OBJECT_TEXTURE1D (Texture1D)
  { 2, MipsTrue,  SampleFalse }, // AR_OBJECT_TEXTURE1D_ARRAY (Texture1DArray)
  { 2, MipsTrue,  SampleFalse }, // AR_OBJECT_TEXTURE2D (Texture2D)
  { 3, MipsTrue,  SampleFalse }, // AR_OBJECT_TEXTURE2D_ARRAY (Texture2DArray)
  { 3, MipsTrue,  SampleFalse }, // AR_OBJECT_TEXTURE3D (Texture3D)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_TEXTURECUBE (TextureCube)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_TEXTURECUBE_ARRAY (TextureCubeArray)
  { 2, MipsFalse, SampleTrue  }, // AR_OBJECT_TEXTURE2DMS (Texture2DMS)
  { 3, MipsFalse, SampleTrue  }, // AR_OBJECT_TEXTURE2DMS_ARRAY (Texture2DMSArray)

  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_SAMPLER (SamplerState)
  //AR_OBJECT_SAMPLER1D,
  //AR_OBJECT_SAMPLER2D,
  //AR_OBJECT_SAMPLER3D,
  //AR_OBJECT_SAMPLERCUBE,
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_SAMPLERCOMPARISON (SamplerComparison)

  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_POINTSTREAM (PointStream)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_LINESTREAM (LineStream)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_TRIANGLESTREAM (TriangleStream)

  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_INPUTPATCH (InputPatch)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_OUTPUTPATCH (OutputPatch)

  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_RWTEXTURE1D (RWTexture1D)
  { 2, MipsFalse, SampleFalse }, // AR_OBJECT_RWTEXTURE1D_ARRAY (RWTexture1DArray)
  { 2, MipsFalse, SampleFalse }, // AR_OBJECT_RWTEXTURE2D (RWTexture2D)
  { 3, MipsFalse, SampleFalse }, // AR_OBJECT_RWTEXTURE2D_ARRAY (RWTexture2DArray)
  { 3, MipsFalse, SampleFalse }, // AR_OBJECT_RWTEXTURE3D (RWTexture3D)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_RWBUFFER (RWBuffer)

  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_BYTEADDRESS_BUFFER (ByteAddressBuffer)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_RWBYTEADDRESS_BUFFER (RWByteAddressBuffer)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_STRUCTURED_BUFFER (StructuredBuffer)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_RWSTRUCTURED_BUFFER (RWStructuredBuffer)
  // AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC,
  // AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME,
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_APPEND_STRUCTURED_BUFFER (AppendStructuredBuffer)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_CONSUME_STRUCTURED_BUFFER (ConsumeStructuredBuffer)

  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_ROVBUFFER (ROVBuffer)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_ROVBYTEADDRESS_BUFFER (ROVByteAddressBuffer)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_ROVSTRUCTURED_BUFFER (ROVStructuredBuffer)
  { 1, MipsFalse, SampleFalse }, // AR_OBJECT_ROVTEXTURE1D (ROVTexture1D)
  { 2, MipsFalse, SampleFalse }, // AR_OBJECT_ROVTEXTURE1D_ARRAY (ROVTexture1DArray)
  { 2, MipsFalse, SampleFalse }, // AR_OBJECT_ROVTEXTURE2D (ROVTexture2D)
  { 3, MipsFalse, SampleFalse }, // AR_OBJECT_ROVTEXTURE2D_ARRAY (ROVTexture2DArray)
  { 3, MipsFalse, SampleFalse }, // AR_OBJECT_ROVTEXTURE3D (ROVTexture3D)

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_VK_SUBPASS_INPUT (SubpassInput)
  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_VK_SUBPASS_INPUT_MS (SubpassInputMS)
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  { 0, MipsFalse, SampleFalse }, // AR_OBJECT_LEGACY_EFFECT (legacy effect objects)
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_WAVE
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_RAY_DESC
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_ACCELERATION_STRUCT
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES

  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_STATE_OBJECT_CONFIG,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_GLOBAL_ROOT_SIGNATURE,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_LOCAL_ROOT_SIGNATURE,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_RAYTRACING_SHADER_CONFIG,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_RAYTRACING_PIPELINE_CONFIG,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_TRIANGLE_HIT_GROUP,
  { 0, MipsFalse, SampleFalse },  // AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP,

};

C_ASSERT(_countof(g_ArBasicKindsAsTypes) == _countof(g_ArBasicKindsSubscripts));

// Type names for ArBasicKind values.
static
const char* g_ArBasicTypeNames[] =
{
  "bool", "float", "half", "half", "float", "double",
  "int", "sbyte", "byte", "short", "ushort",
  "int", "uint", "long", "ulong",
  "min10float", "min16float",
  "min12int", "min16int", "min16uint",
  "enum",

  "<count>",
  "<none>",
  "<unknown>",
  "<nocast>",
  "<pointer>",
  "enum class",

  "null",
  "literal string",
  "string",
  // "texture",
  "Texture1D",
  "Texture1DArray",
  "Texture2D",
  "Texture2DArray",
  "Texture3D",
  "TextureCube",
  "TextureCubeArray",
  "Texture2DMS",
  "Texture2DMSArray",
  "SamplerState",
  "sampler1D",
  "sampler2D",
  "sampler3D",
  "samplerCUBE",
  "SamplerComparisonState",
  "Buffer",
  "RenderTargetView",
  "DepthStencilView",
  "ComputeShader",
  "DomainShader",
  "GeometryShader",
  "HullShader",
  "PixelShader",
  "VertexShader",
  "pixelfragment",
  "vertexfragment",
  "StateBlock",
  "Rasterizer",
  "DepthStencil",
  "Blend",
  "PointStream",
  "LineStream",
  "TriangleStream",
  "InputPatch",
  "OutputPatch",
  "RWTexture1D",
  "RWTexture1DArray",
  "RWTexture2D",
  "RWTexture2DArray",
  "RWTexture3D",
  "RWBuffer",
  "ByteAddressBuffer",
  "RWByteAddressBuffer",
  "StructuredBuffer",
  "RWStructuredBuffer",
  "RWStructuredBuffer(Incrementable)",
  "RWStructuredBuffer(Decrementable)",
  "AppendStructuredBuffer",
  "ConsumeStructuredBuffer",

  "ConstantBuffer",
  "TextureBuffer",

  "RasterizerOrderedBuffer",
  "RasterizerOrderedByteAddressBuffer",
  "RasterizerOrderedStructuredBuffer",
  "RasterizerOrderedTexture1D",
  "RasterizerOrderedTexture1DArray",
  "RasterizerOrderedTexture2D",
  "RasterizerOrderedTexture2DArray",
  "RasterizerOrderedTexture3D",

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  "SubpassInput",
  "SubpassInputMS",
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  "<internal inner type object>",

  "deprecated effect object",
  "wave_t",
  "RayDesc",
  "RaytracingAccelerationStructure",
  "user defined type",
  "BuiltInTriangleIntersectionAttributes",

  // subobjects
  "StateObjectConfig",
  "GlobalRootSignature",
  "LocalRootSignature",
  "SubobjectToExportsAssociation", 
  "RaytracingShaderConfig",
  "RaytracingPipelineConfig",
  "TriangleHitGroup",
  "ProceduralPrimitiveHitGroup"
};

C_ASSERT(_countof(g_ArBasicTypeNames) == AR_BASIC_MAXIMUM_COUNT);

static bool IsValidBasicKind(ArBasicKind kind) {
  return kind != AR_BASIC_COUNT &&
    kind != AR_BASIC_NONE &&
    kind != AR_BASIC_UNKNOWN &&
    kind != AR_BASIC_NOCAST &&
    kind != AR_BASIC_POINTER &&
    kind != AR_OBJECT_RENDERTARGETVIEW &&
    kind != AR_OBJECT_DEPTHSTENCILVIEW &&
    kind != AR_OBJECT_COMPUTESHADER &&
    kind != AR_OBJECT_DOMAINSHADER &&
    kind != AR_OBJECT_GEOMETRYSHADER &&
    kind != AR_OBJECT_HULLSHADER &&
    kind != AR_OBJECT_PIXELSHADER &&
    kind != AR_OBJECT_VERTEXSHADER &&
    kind != AR_OBJECT_PIXELFRAGMENT &&
    kind != AR_OBJECT_VERTEXFRAGMENT;
}
// kind should never be a flag value or effects framework type - we simply do not expect to deal with these
#define DXASSERT_VALIDBASICKIND(kind) \
  DXASSERT(IsValidBasicKind(kind), "otherwise caller is using a special flag or an unsupported kind value");

static
const char* g_DeprecatedEffectObjectNames[] =
{
  // These are case insensitive in fxc, but we'll just create two case aliases
  // to capture the majority of cases
  "texture",        "Texture",
  "pixelshader",    "PixelShader",
  "vertexshader",   "VertexShader",

  // These are case sensitive in fxc
  "pixelfragment",    // 13
  "vertexfragment",   // 14
  "ComputeShader",    // 13
  "DomainShader",     // 12
  "GeometryShader",   // 14
  "HullShader",       // 10
  "BlendState",       // 10
  "DepthStencilState",// 17
  "DepthStencilView", // 16
  "RasterizerState",  // 15
  "RenderTargetView", // 16
};

static hlsl::ParameterModifier
ParamModsFromIntrinsicArg(const HLSL_INTRINSIC_ARGUMENT *pArg) {
  if (pArg->qwUsage == AR_QUAL_IN_OUT) {
    return hlsl::ParameterModifier(hlsl::ParameterModifier::Kind::InOut);
  }
  if (pArg->qwUsage == AR_QUAL_OUT) {
    return hlsl::ParameterModifier(hlsl::ParameterModifier::Kind::Out);
  }
  DXASSERT(pArg->qwUsage & AR_QUAL_IN, "else usage is incorrect");
  return hlsl::ParameterModifier(hlsl::ParameterModifier::Kind::In);
}

static void InitParamMods(const HLSL_INTRINSIC *pIntrinsic,
                          SmallVectorImpl<hlsl::ParameterModifier> &paramMods) {
  // The first argument is the return value, which isn't included.
  for (UINT i = 1; i < pIntrinsic->uNumArgs; ++i) {
    paramMods.push_back(ParamModsFromIntrinsicArg(&pIntrinsic->pArgs[i]));
  }
}

static bool IsAtomicOperation(IntrinsicOp op) {
  switch (op) {
  case IntrinsicOp::IOP_InterlockedAdd:
  case IntrinsicOp::IOP_InterlockedAnd:
  case IntrinsicOp::IOP_InterlockedCompareExchange:
  case IntrinsicOp::IOP_InterlockedCompareStore:
  case IntrinsicOp::IOP_InterlockedExchange:
  case IntrinsicOp::IOP_InterlockedMax:
  case IntrinsicOp::IOP_InterlockedMin:
  case IntrinsicOp::IOP_InterlockedOr:
  case IntrinsicOp::IOP_InterlockedXor:
  case IntrinsicOp::MOP_InterlockedAdd:
  case IntrinsicOp::MOP_InterlockedAnd:
  case IntrinsicOp::MOP_InterlockedCompareExchange:
  case IntrinsicOp::MOP_InterlockedCompareStore:
  case IntrinsicOp::MOP_InterlockedExchange:
  case IntrinsicOp::MOP_InterlockedMax:
  case IntrinsicOp::MOP_InterlockedMin:
  case IntrinsicOp::MOP_InterlockedOr:
  case IntrinsicOp::MOP_InterlockedXor:
    return true;
  default:
    return false;
  }
}

static bool IsBuiltinTable(LPCSTR tableName) {
  return tableName == kBuiltinIntrinsicTableName;
}

static void AddHLSLIntrinsicAttr(FunctionDecl *FD, ASTContext &context,
                              LPCSTR tableName, LPCSTR lowering,
                              const HLSL_INTRINSIC *pIntrinsic) {
  unsigned opcode = (unsigned)pIntrinsic->Op;
  if (HasUnsignedOpcode(opcode) && IsBuiltinTable(tableName)) {
    QualType Ty = FD->getReturnType();
    if (pIntrinsic->iOverloadParamIndex != -1) {
      const FunctionProtoType *FT =
          FD->getFunctionType()->getAs<FunctionProtoType>();
      Ty = FT->getParamType(pIntrinsic->iOverloadParamIndex);
    }

    // TODO: refine the code for getting element type
    if (const ExtVectorType *VecTy = hlsl::ConvertHLSLVecMatTypeToExtVectorType(context, Ty)) {
      Ty = VecTy->getElementType();
    }
    if (Ty->isUnsignedIntegerType()) {
      opcode = hlsl::GetUnsignedOpcode(opcode);
    }
  }
  FD->addAttr(HLSLIntrinsicAttr::CreateImplicit(context, tableName, lowering, opcode));
  if (pIntrinsic->bReadNone)
    FD->addAttr(ConstAttr::CreateImplicit(context));
  if (pIntrinsic->bReadOnly)
    FD->addAttr(PureAttr::CreateImplicit(context));
}

static
FunctionDecl *AddHLSLIntrinsicFunction(
    ASTContext &context, _In_ NamespaceDecl *NS,
    LPCSTR tableName, LPCSTR lowering,
    _In_ const HLSL_INTRINSIC *pIntrinsic,
    _In_count_(functionArgTypeCount) QualType *functionArgQualTypes,
    _In_range_(0, g_MaxIntrinsicParamCount - 1) size_t functionArgTypeCount) {
  DXASSERT(functionArgTypeCount - 1 <= g_MaxIntrinsicParamCount,
           "otherwise g_MaxIntrinsicParamCount should be larger");
  DeclContext *currentDeclContext = context.getTranslationUnitDecl();

  SmallVector<hlsl::ParameterModifier, g_MaxIntrinsicParamCount> paramMods;
  InitParamMods(pIntrinsic, paramMods);

  // Change dest address into reference type for atomic.
  if (IsBuiltinTable(tableName)) {
    if (IsAtomicOperation(static_cast<IntrinsicOp>(pIntrinsic->Op))) {
      DXASSERT(functionArgTypeCount > kAtomicDstOperandIdx,
               "else operation was misrecognized");
      functionArgQualTypes[kAtomicDstOperandIdx] =
          context.getLValueReferenceType(functionArgQualTypes[kAtomicDstOperandIdx]);
    }
  }

  for (size_t i = 1; i < functionArgTypeCount; i++) {
    // Change out/inout param to reference type.
    if (paramMods[i-1].isAnyOut()) {
      QualType Ty = functionArgQualTypes[i];
      // Aggregate type will be indirect param convert to pointer type.
      // Don't need add reference for it.
      if ((!Ty->isArrayType() && !Ty->isRecordType()) ||
          hlsl::IsHLSLVecMatType(Ty)) {
        functionArgQualTypes[i] = context.getLValueReferenceType(Ty);
      }
    }
  }

  IdentifierInfo &functionId = context.Idents.get(
      StringRef(pIntrinsic->pArgs[0].pName), tok::TokenKind::identifier);
  DeclarationName functionName(&functionId);
  QualType functionType = context.getFunctionType(
      functionArgQualTypes[0],
      ArrayRef<QualType>(functionArgQualTypes + 1,
                         functionArgQualTypes + functionArgTypeCount),
      clang::FunctionProtoType::ExtProtoInfo(), paramMods);
  FunctionDecl *functionDecl = FunctionDecl::Create(
      context, currentDeclContext, NoLoc,
      DeclarationNameInfo(functionName, NoLoc), functionType, nullptr,
      StorageClass::SC_Extern, InlineSpecifiedFalse, HasWrittenPrototypeTrue);
  currentDeclContext->addDecl(functionDecl);

  functionDecl->setLexicalDeclContext(currentDeclContext);
  // put under hlsl namespace
  functionDecl->setDeclContext(NS);
  // Add intrinsic attribute
  AddHLSLIntrinsicAttr(functionDecl, context, tableName, lowering, pIntrinsic);

  ParmVarDecl *paramDecls[g_MaxIntrinsicParamCount];
  for (size_t i = 1; i < functionArgTypeCount; i++) {
    IdentifierInfo &parameterId = context.Idents.get(
        StringRef(pIntrinsic->pArgs[i].pName), tok::TokenKind::identifier);
    ParmVarDecl *paramDecl =
        ParmVarDecl::Create(context, functionDecl, NoLoc, NoLoc, &parameterId,
                            functionArgQualTypes[i], nullptr,
                            StorageClass::SC_None, nullptr, paramMods[i - 1]);
    functionDecl->addDecl(paramDecl);
    paramDecls[i - 1] = paramDecl;
  }

  functionDecl->setParams(
      ArrayRef<ParmVarDecl *>(paramDecls, functionArgTypeCount - 1));
  functionDecl->setImplicit(true);

  return functionDecl;
}

/// <summary>
/// Checks whether the specified expression is a (possibly parenthesized) comma operator.
/// </summary>
static
bool IsExpressionBinaryComma(_In_ const Expr* expr)
{
  DXASSERT_NOMSG(expr != nullptr);
  expr = expr->IgnoreParens();
  return
    expr->getStmtClass() == Expr::StmtClass::BinaryOperatorClass &&
    cast<BinaryOperator>(expr)->getOpcode() == BinaryOperatorKind::BO_Comma;
}

/// <summary>
/// Silences diagnostics for the initialization sequence, typically because they have already
/// been emitted.
/// </summary>
static
void SilenceSequenceDiagnostics(_Inout_ InitializationSequence* initSequence)
{
  DXASSERT_NOMSG(initSequence != nullptr);
  initSequence->SetFailed(InitializationSequence::FK_ListInitializationFailed);
}

class UsedIntrinsic
{
public:
  static int compareArgs(const QualType& LHS, const QualType& RHS)
  {
    // The canonical representations are unique'd in an ASTContext, and so these
    // should be stable.
    return RHS.getTypePtr() - LHS.getTypePtr();
  }

  static int compareIntrinsic(const HLSL_INTRINSIC* LHS, const HLSL_INTRINSIC* RHS)
  {
    // The intrinsics are defined in a single static table, and so should be stable.
    return RHS - LHS;
  }

  int compare(const UsedIntrinsic& other) const
  {
    // Check whether it's the same instance.
    if (this == &other) return 0;

    int result = compareIntrinsic(m_intrinsicSource, other.m_intrinsicSource);
    if (result != 0) return result;

    // At this point, it's the exact same intrinsic name.
    // Compare the arguments for ordering then.
    DXASSERT(m_argLength == other.m_argLength, "intrinsics aren't overloaded on argument count, so we should never create a key with different #s");
    for (size_t i = 0; i < m_argLength; i++) {
      int argComparison = compareArgs(m_args[i], other.m_args[i]);
      if (argComparison != 0) return argComparison;
    }

    // Exactly the same.
    return 0;
  }

public:
  UsedIntrinsic(const HLSL_INTRINSIC* intrinsicSource, _In_count_(argCount) QualType* args, size_t argCount)
    : m_argLength(argCount), m_intrinsicSource(intrinsicSource), m_functionDecl(nullptr)
  {
    std::copy(args, args + argCount, m_args);
  }

  void setFunctionDecl(FunctionDecl* value) const
  {
    DXASSERT(value != nullptr, "no reason to clear this out");
    DXASSERT(m_functionDecl == nullptr, "otherwise cached value is being invaldiated");
    m_functionDecl = value;
  }
  FunctionDecl* getFunctionDecl() const { return m_functionDecl; }

  bool operator==(const UsedIntrinsic& other) const
  {
    return compare(other) == 0;
  }

  bool operator<(const UsedIntrinsic& other) const
  {
    return compare(other) < 0;
  }

private:
  QualType m_args[g_MaxIntrinsicParamCount+1];
  size_t m_argLength;
  const HLSL_INTRINSIC* m_intrinsicSource;
  mutable FunctionDecl* m_functionDecl;
};

template <typename T>
inline void AssignOpt(T value, _Out_opt_ T* ptr)
{
  if (ptr != nullptr)
  {
    *ptr = value;
  }
}

static bool CombineBasicTypes(ArBasicKind LeftKind,
                              ArBasicKind RightKind,
                              _Out_ ArBasicKind* pOutKind)
{
  if ((LeftKind < 0 || LeftKind >= AR_BASIC_COUNT) ||
    (RightKind < 0 || RightKind >= AR_BASIC_COUNT)) {
    return false;
  }

  if (LeftKind == RightKind) {
    *pOutKind = LeftKind;
    return true;
  }

  UINT uLeftProps = GetBasicKindProps(LeftKind);
  UINT uRightProps = GetBasicKindProps(RightKind);
  UINT uBits = GET_BPROP_BITS(uLeftProps) > GET_BPROP_BITS(uRightProps) ?
               GET_BPROP_BITS(uLeftProps) : GET_BPROP_BITS(uRightProps);
  UINT uBothFlags = uLeftProps & uRightProps;
  UINT uEitherFlags = uLeftProps | uRightProps;

  // Notes: all numeric types have either BPROP_FLOATING or BPROP_INTEGER (even bool)
  //        unsigned only applies to non-literal ints, not bool or enum
  //        literals, bool, and enum are all BPROP_BITS0
  if (uBothFlags & BPROP_BOOLEAN) {
    *pOutKind = AR_BASIC_BOOL;
    return true;
  }

  bool bFloatResult = 0 != (uEitherFlags & BPROP_FLOATING);
  if (uBothFlags & BPROP_LITERAL) {
    *pOutKind = bFloatResult ? AR_BASIC_LITERAL_FLOAT : AR_BASIC_LITERAL_INT;
    return true;
  }

  // Starting approximation of result properties:
  // - float if either are float, otherwise int (see Notes above)
  // - min/partial precision if both have same flag
  // - if not float, add unsigned if either is unsigned
  UINT uResultFlags =
    (uBothFlags & (BPROP_INTEGER | BPROP_MIN_PRECISION | BPROP_PARTIAL_PRECISION)) |
    (uEitherFlags & BPROP_FLOATING) |
    (!bFloatResult ? (uEitherFlags & BPROP_UNSIGNED) : 0);

  // If one is literal/bool/enum, use min/partial precision from the other
  if (uEitherFlags & (BPROP_LITERAL | BPROP_BOOLEAN | BPROP_ENUM)) {
    uResultFlags |= uEitherFlags & (BPROP_MIN_PRECISION | BPROP_PARTIAL_PRECISION);
  }

  // Now if we have partial precision, we know the result must be half
  if (uResultFlags & BPROP_PARTIAL_PRECISION) {
    *pOutKind = AR_BASIC_FLOAT32_PARTIAL_PRECISION;
    return true;
  }

  // uBits are already initialized to max of either side, so now:
  // if only one is float, get result props from float side
  //  min16float + int -> min16float
  //  also take min precision from that side
  if (bFloatResult && 0 == (uBothFlags & BPROP_FLOATING)) {
    uResultFlags = (uLeftProps & BPROP_FLOATING) ? uLeftProps : uRightProps;
    uBits = GET_BPROP_BITS(uResultFlags);
    uResultFlags &= ~BPROP_LITERAL;
  }

  bool bMinPrecisionResult = uResultFlags & BPROP_MIN_PRECISION;

  // if uBits is 0 here, upgrade to 32-bits
  // this happens if bool, literal or enum on both sides,
  // or if float came from literal side
  if (uBits == BPROP_BITS0)
    uBits = BPROP_BITS32;

  DXASSERT(uBits != BPROP_BITS0, "CombineBasicTypes: uBits should not be zero at this point");
  DXASSERT(uBits != BPROP_BITS8, "CombineBasicTypes: 8-bit types not supported at this time");

  if (bMinPrecisionResult) {
    DXASSERT(uBits < BPROP_BITS32, "CombineBasicTypes: min-precision result must be less than 32-bits");
  } else {
    DXASSERT(uBits > BPROP_BITS12, "CombineBasicTypes: 10 or 12 bit result must be min precision");
  }
  if (bFloatResult) {
    DXASSERT(uBits != BPROP_BITS12, "CombineBasicTypes: 12-bit result must be int");
  } else {
    DXASSERT(uBits != BPROP_BITS10, "CombineBasicTypes: 10-bit result must be float");
  }
  if (uBits == BPROP_BITS12) {
    DXASSERT(!(uResultFlags & BPROP_UNSIGNED), "CombineBasicTypes: 12-bit result must not be unsigned");
  }

  if (bFloatResult) {
    switch (uBits) {
    case BPROP_BITS10:
      *pOutKind = AR_BASIC_MIN10FLOAT;
      break;
    case BPROP_BITS16:
      *pOutKind = bMinPrecisionResult ? AR_BASIC_MIN16FLOAT : AR_BASIC_FLOAT16;
      break;
    case BPROP_BITS32:
      *pOutKind = AR_BASIC_FLOAT32;
      break;
    case BPROP_BITS64:
      *pOutKind = AR_BASIC_FLOAT64;
      break;
    default:
      DXASSERT(false, "Unexpected bit count for float result");
      break;
    }
  } else {
    // int or unsigned int
    switch (uBits) {
    case BPROP_BITS12:
      *pOutKind = AR_BASIC_MIN12INT;
      break;
    case BPROP_BITS16:
      if (uResultFlags & BPROP_UNSIGNED)
        *pOutKind = bMinPrecisionResult ? AR_BASIC_MIN16UINT : AR_BASIC_UINT16;
      else
        *pOutKind = bMinPrecisionResult ? AR_BASIC_MIN16INT : AR_BASIC_INT16;
      break;
    case BPROP_BITS32:
      *pOutKind = (uResultFlags & BPROP_UNSIGNED) ? AR_BASIC_UINT32 : AR_BASIC_INT32;
      break;
    case BPROP_BITS64:
      *pOutKind = (uResultFlags & BPROP_UNSIGNED) ? AR_BASIC_UINT64 : AR_BASIC_INT64;
      break;
    default:
      DXASSERT(false, "Unexpected bit count for int result");
      break;
    }
  }

  return true;
}

class UsedIntrinsicStore : public std::set<UsedIntrinsic>
{
};

static
void GetIntrinsicMethods(ArBasicKind kind, _Outptr_result_buffer_(*intrinsicCount) const HLSL_INTRINSIC** intrinsics, _Out_ size_t* intrinsicCount)
{
  DXASSERT_NOMSG(intrinsics != nullptr);
  DXASSERT_NOMSG(intrinsicCount != nullptr);

  switch (kind)
  {
  case AR_OBJECT_TRIANGLESTREAM:
  case AR_OBJECT_POINTSTREAM:
  case AR_OBJECT_LINESTREAM:
    *intrinsics = g_StreamMethods;
    *intrinsicCount = _countof(g_StreamMethods);
    break;
  case AR_OBJECT_TEXTURE1D:
    *intrinsics = g_Texture1DMethods;
    *intrinsicCount = _countof(g_Texture1DMethods);
    break;
  case AR_OBJECT_TEXTURE1D_ARRAY:
    *intrinsics = g_Texture1DArrayMethods;
    *intrinsicCount = _countof(g_Texture1DArrayMethods);
    break;
  case AR_OBJECT_TEXTURE2D:
    *intrinsics = g_Texture2DMethods;
    *intrinsicCount = _countof(g_Texture2DMethods);
    break;
  case AR_OBJECT_TEXTURE2DMS:
    *intrinsics = g_Texture2DMSMethods;
    *intrinsicCount = _countof(g_Texture2DMSMethods);
    break;
  case AR_OBJECT_TEXTURE2D_ARRAY:
    *intrinsics = g_Texture2DArrayMethods;
    *intrinsicCount = _countof(g_Texture2DArrayMethods);
    break;
  case AR_OBJECT_TEXTURE2DMS_ARRAY:
    *intrinsics = g_Texture2DArrayMSMethods;
    *intrinsicCount = _countof(g_Texture2DArrayMSMethods);
    break;
  case AR_OBJECT_TEXTURE3D:
    *intrinsics = g_Texture3DMethods;
    *intrinsicCount = _countof(g_Texture3DMethods);
    break;
  case AR_OBJECT_TEXTURECUBE:
    *intrinsics = g_TextureCUBEMethods;
    *intrinsicCount = _countof(g_TextureCUBEMethods);
    break;
  case AR_OBJECT_TEXTURECUBE_ARRAY:
    *intrinsics = g_TextureCUBEArrayMethods;
    *intrinsicCount = _countof(g_TextureCUBEArrayMethods);
    break;
  case AR_OBJECT_BUFFER:
    *intrinsics = g_BufferMethods;
    *intrinsicCount = _countof(g_BufferMethods);
    break;
  case AR_OBJECT_RWTEXTURE1D:
  case AR_OBJECT_ROVTEXTURE1D:
    *intrinsics = g_RWTexture1DMethods;
    *intrinsicCount = _countof(g_RWTexture1DMethods);
    break;
  case AR_OBJECT_RWTEXTURE1D_ARRAY:
  case AR_OBJECT_ROVTEXTURE1D_ARRAY:
    *intrinsics = g_RWTexture1DArrayMethods;
    *intrinsicCount = _countof(g_RWTexture1DArrayMethods);
    break;
  case AR_OBJECT_RWTEXTURE2D:
  case AR_OBJECT_ROVTEXTURE2D:
    *intrinsics = g_RWTexture2DMethods;
    *intrinsicCount = _countof(g_RWTexture2DMethods);
    break;
  case AR_OBJECT_RWTEXTURE2D_ARRAY:
  case AR_OBJECT_ROVTEXTURE2D_ARRAY:
    *intrinsics = g_RWTexture2DArrayMethods;
    *intrinsicCount = _countof(g_RWTexture2DArrayMethods);
    break;
  case AR_OBJECT_RWTEXTURE3D:
  case AR_OBJECT_ROVTEXTURE3D:
    *intrinsics = g_RWTexture3DMethods;
    *intrinsicCount = _countof(g_RWTexture3DMethods);
    break;
  case AR_OBJECT_RWBUFFER:
  case AR_OBJECT_ROVBUFFER:
    *intrinsics = g_RWBufferMethods;
    *intrinsicCount = _countof(g_RWBufferMethods);
    break;
  case AR_OBJECT_BYTEADDRESS_BUFFER:
    *intrinsics = g_ByteAddressBufferMethods;
    *intrinsicCount = _countof(g_ByteAddressBufferMethods);
    break;
  case AR_OBJECT_RWBYTEADDRESS_BUFFER:
  case AR_OBJECT_ROVBYTEADDRESS_BUFFER:
    *intrinsics = g_RWByteAddressBufferMethods;
    *intrinsicCount = _countof(g_RWByteAddressBufferMethods);
    break;
  case AR_OBJECT_STRUCTURED_BUFFER:
    *intrinsics = g_StructuredBufferMethods;
    *intrinsicCount = _countof(g_StructuredBufferMethods);
    break;
  case AR_OBJECT_RWSTRUCTURED_BUFFER:
  case AR_OBJECT_ROVSTRUCTURED_BUFFER:
    *intrinsics = g_RWStructuredBufferMethods;
    *intrinsicCount = _countof(g_RWStructuredBufferMethods);
    break;
  case AR_OBJECT_APPEND_STRUCTURED_BUFFER:
    *intrinsics = g_AppendStructuredBufferMethods;
    *intrinsicCount = _countof(g_AppendStructuredBufferMethods);
    break;
  case AR_OBJECT_CONSUME_STRUCTURED_BUFFER:
    *intrinsics = g_ConsumeStructuredBufferMethods;
    *intrinsicCount = _countof(g_ConsumeStructuredBufferMethods);
    break;
  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  case AR_OBJECT_VK_SUBPASS_INPUT:
    *intrinsics = g_VkSubpassInputMethods;
    *intrinsicCount = _countof(g_VkSubpassInputMethods);
    break;
  case AR_OBJECT_VK_SUBPASS_INPUT_MS:
    *intrinsics = g_VkSubpassInputMSMethods;
    *intrinsicCount = _countof(g_VkSubpassInputMSMethods);
    break;
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends
  default:
    *intrinsics = nullptr;
    *intrinsicCount = 0;
    break;
  }
}

static
bool IsRowOrColumnVariable(size_t value)
{
  return IA_SPECIAL_BASE <= value && value <= (IA_SPECIAL_BASE + IA_SPECIAL_SLOTS - 1);
}

static
bool DoesComponentTypeAcceptMultipleTypes(LEGAL_INTRINSIC_COMPTYPES value)
{
  return
    value == LICOMPTYPE_ANY_INT ||        // signed or unsigned ints
    value == LICOMPTYPE_ANY_INT32 ||      // signed or unsigned ints
    value == LICOMPTYPE_ANY_FLOAT ||      // float or double
    value == LICOMPTYPE_FLOAT_LIKE ||     // float or min16
    value == LICOMPTYPE_FLOAT_DOUBLE ||   // float or double
    value == LICOMPTYPE_NUMERIC ||        // all sorts of numbers
    value == LICOMPTYPE_NUMERIC32 ||      // all sorts of numbers
    value == LICOMPTYPE_NUMERIC32_ONLY || // all sorts of numbers
    value == LICOMPTYPE_ANY;              // any time
}

static
bool DoesComponentTypeAcceptMultipleTypes(BYTE value)
{
  return DoesComponentTypeAcceptMultipleTypes(static_cast<LEGAL_INTRINSIC_COMPTYPES>(value));
}

static
bool DoesLegalTemplateAcceptMultipleTypes(LEGAL_INTRINSIC_TEMPLATES value)
{
  // Note that LITEMPLATE_OBJECT can accept different types, but it
  // specifies a single 'layout'. In practice, this information is used
  // together with a component type that specifies a single object.

  return value == LITEMPLATE_ANY; // Any layout
}

static
bool DoesLegalTemplateAcceptMultipleTypes(BYTE value)
{
  return DoesLegalTemplateAcceptMultipleTypes(static_cast<LEGAL_INTRINSIC_TEMPLATES>(value));
}

static
bool TemplateHasDefaultType(ArBasicKind kind)
{
  switch (kind) {
  case AR_OBJECT_BUFFER:
  case AR_OBJECT_TEXTURE1D:
  case AR_OBJECT_TEXTURE2D:
  case AR_OBJECT_TEXTURE3D:
  case AR_OBJECT_TEXTURE1D_ARRAY:
  case AR_OBJECT_TEXTURE2D_ARRAY:
  case AR_OBJECT_TEXTURECUBE:
  case AR_OBJECT_TEXTURECUBE_ARRAY:
  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  case AR_OBJECT_VK_SUBPASS_INPUT:
  case AR_OBJECT_VK_SUBPASS_INPUT_MS:
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends
    return true;
  default:
    // Objects with default types return true. Everything else is false.
    return false;
  }
}

/// <summary>
/// Use this class to iterate over intrinsic definitions that come from an external source.
/// </summary>
class IntrinsicTableDefIter
{
private:
  StringRef _typeName;
  StringRef _functionName;
  llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2>& _tables;
  const HLSL_INTRINSIC* _tableIntrinsic;
  UINT64 _tableLookupCookie;
  unsigned _tableIndex;
  unsigned _argCount;
  bool _firstChecked;

  IntrinsicTableDefIter(
    llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2>& tables,
    StringRef typeName,
    StringRef functionName,
    unsigned argCount) :
    _typeName(typeName), _functionName(functionName), _tables(tables),
    _tableIntrinsic(nullptr), _tableLookupCookie(0), _tableIndex(0),
    _argCount(argCount), _firstChecked(false)
  {
  }

  void CheckForIntrinsic() {
    if (_tableIndex >= _tables.size()) {
      return;
    }

    _firstChecked = true;

    // TODO: review this - this will allocate at least once per string
    CA2WEX<> typeName(_typeName.str().c_str(), CP_UTF8);
    CA2WEX<> functionName(_functionName.str().c_str(), CP_UTF8);

    if (FAILED(_tables[_tableIndex]->LookupIntrinsic(
            typeName, functionName, &_tableIntrinsic, &_tableLookupCookie))) {
      _tableLookupCookie = 0;
      _tableIntrinsic = nullptr;
    }
  }

  void MoveToNext() {
    for (;;) {
      // If we don't have an intrinsic, try the following table.
      if (_firstChecked && _tableIntrinsic == nullptr) {
        _tableIndex++;
      }

      CheckForIntrinsic();
      if (_tableIndex == _tables.size() ||
          (_tableIntrinsic != nullptr &&
           _tableIntrinsic->uNumArgs ==
               (_argCount + 1))) // uNumArgs includes return
        break;
    }
  }

public:
  static IntrinsicTableDefIter CreateStart(llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2>& tables,
    StringRef typeName,
    StringRef functionName,
    unsigned argCount)
  {
    IntrinsicTableDefIter result(tables, typeName, functionName, argCount);
    return result;
  }

  static IntrinsicTableDefIter CreateEnd(llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2>& tables)
  {
    IntrinsicTableDefIter result(tables, StringRef(), StringRef(), 0);
    result._tableIndex = tables.size();
    return result;
  }

  bool operator!=(const IntrinsicTableDefIter& other)
  {
    if (!_firstChecked) {
      MoveToNext();
    }
    return _tableIndex != other._tableIndex; // More things could be compared but we only match end.
  }

  const HLSL_INTRINSIC* operator*()
  {
    DXASSERT(_firstChecked, "otherwise deref without comparing to end");
    return _tableIntrinsic;
  }

  LPCSTR GetTableName()
  {
    LPCSTR tableName = nullptr;
    if (FAILED(_tables[_tableIndex]->GetTableName(&tableName))) {
      return nullptr;
    }
    return tableName;
  }

  LPCSTR GetLoweringStrategy()
  {
    LPCSTR lowering = nullptr;
    if (FAILED(_tables[_tableIndex]->GetLoweringStrategy(_tableIntrinsic->Op, &lowering))) {
      return nullptr;
    }
    return lowering;
  }

  IntrinsicTableDefIter& operator++()
  {
    MoveToNext();
    return *this;
  }
};

/// <summary>
/// Use this class to iterate over intrinsic definitions that have the same name and parameter count.
/// </summary>
class IntrinsicDefIter
{
  const HLSL_INTRINSIC* _current;
  const HLSL_INTRINSIC* _end;
  IntrinsicTableDefIter _tableIter;

  IntrinsicDefIter(const HLSL_INTRINSIC* value, const HLSL_INTRINSIC* end, IntrinsicTableDefIter tableIter) :
    _current(value), _end(end), _tableIter(tableIter)
  { }

public:
  static IntrinsicDefIter CreateStart(const HLSL_INTRINSIC* table, size_t count, const HLSL_INTRINSIC* start, IntrinsicTableDefIter tableIter)
  {
    return IntrinsicDefIter(start, table + count, tableIter);
  }

  static IntrinsicDefIter CreateEnd(const HLSL_INTRINSIC* table, size_t count, IntrinsicTableDefIter tableIter)
  {
    return IntrinsicDefIter(table + count, table + count, tableIter);
  }

  bool operator!=(const IntrinsicDefIter& other)
  {
    return _current != other._current || _tableIter.operator!=(other._tableIter);
  }

  const HLSL_INTRINSIC* operator*()
  {
    return (_current != _end) ? _current : *_tableIter;
  }

  LPCSTR GetTableName()
  {
    return (_current != _end) ? kBuiltinIntrinsicTableName : _tableIter.GetTableName();
  }

  LPCSTR GetLoweringStrategy()
  {
    return (_current != _end) ? "" : _tableIter.GetLoweringStrategy();
  }

  IntrinsicDefIter& operator++()
  {
    if (_current != _end) {
      const HLSL_INTRINSIC* next = _current + 1;
      if (next != _end && _current->uNumArgs == next->uNumArgs && 0 == strcmp(_current->pArgs[0].pName, next->pArgs[0].pName)) {
        _current = next;
      }
      else {
        _current = _end;
      }
    } else {
      ++_tableIter;
    }

    return *this;
  }
};

static void AddHLSLSubscriptAttr(Decl *D, ASTContext &context, HLSubscriptOpcode opcode) {
  StringRef group = GetHLOpcodeGroupName(HLOpcodeGroup::HLSubscript);
  D->addAttr(HLSLIntrinsicAttr::CreateImplicit(context, group, "", static_cast<unsigned>(opcode)));
}

static void CreateSimpleField(clang::ASTContext &context, CXXRecordDecl *recordDecl, StringRef Name,
                              QualType Ty, AccessSpecifier access = AccessSpecifier::AS_public) {
  IdentifierInfo &fieldId =
      context.Idents.get(Name, tok::TokenKind::identifier);
  TypeSourceInfo *filedTypeSource = context.getTrivialTypeSourceInfo(Ty, NoLoc);
  const bool MutableFalse = false;
  const InClassInitStyle initStyle = InClassInitStyle::ICIS_NoInit;

  FieldDecl *fieldDecl =
      FieldDecl::Create(context, recordDecl, NoLoc, NoLoc, &fieldId, Ty,
                        filedTypeSource, nullptr, MutableFalse, initStyle);
  fieldDecl->setAccess(access);
  fieldDecl->setImplicit(true);

  recordDecl->addDecl(fieldDecl);
}

// struct RayDesc
//{
//    float3 Origin;
//    float  TMin;
//    float3 Direction;
//    float  TMax;
//};
static CXXRecordDecl *CreateRayDescStruct(clang::ASTContext &context,
                                          QualType float3Ty) {
  DeclContext *currentDeclContext = context.getTranslationUnitDecl();
  IdentifierInfo &rayDesc =
      context.Idents.get(StringRef("RayDesc"), tok::TokenKind::identifier);
  CXXRecordDecl *rayDescDecl = CXXRecordDecl::Create(
      context, TagTypeKind::TTK_Struct, currentDeclContext, NoLoc, NoLoc,
      &rayDesc, nullptr, DelayTypeCreationTrue);
  rayDescDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));
  rayDescDecl->startDefinition();

  QualType floatTy = context.FloatTy;
  // float3 Origin;
  CreateSimpleField(context, rayDescDecl, "Origin", float3Ty);
  // float TMin;
  CreateSimpleField(context, rayDescDecl, "TMin", floatTy);
  // float3 Direction;
  CreateSimpleField(context, rayDescDecl, "Direction", float3Ty);
  // float  TMax;
  CreateSimpleField(context, rayDescDecl, "TMax", floatTy);

  rayDescDecl->completeDefinition();
  // Both declarations need to be present for correct handling.
  currentDeclContext->addDecl(rayDescDecl);
  rayDescDecl->setImplicit(true);
  return rayDescDecl;
}

// struct BuiltInTriangleIntersectionAttributes
// {
//   float2 barycentrics;
// };
static CXXRecordDecl *AddBuiltInTriangleIntersectionAttributes(ASTContext& context, QualType baryType) {
    DeclContext *curDC = context.getTranslationUnitDecl();
    IdentifierInfo &attributesId =
        context.Idents.get(StringRef("BuiltInTriangleIntersectionAttributes"),
            tok::TokenKind::identifier);
    CXXRecordDecl *attributesDecl = CXXRecordDecl::Create(
        context, TagTypeKind::TTK_Struct, curDC, NoLoc, NoLoc,
        &attributesId, nullptr, DelayTypeCreationTrue);
    attributesDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));
    attributesDecl->startDefinition();
    // float2 barycentrics;
    CreateSimpleField(context, attributesDecl, "barycentrics", baryType);
    attributesDecl->completeDefinition();
    attributesDecl->setImplicit(true);
    curDC->addDecl(attributesDecl);
    return attributesDecl;
}

//
// Subobjects

static CXXRecordDecl *StartSubobjectDecl(ASTContext& context, const char *name) {
  IdentifierInfo &id = context.Idents.get(StringRef(name), tok::TokenKind::identifier);
  CXXRecordDecl *decl = CXXRecordDecl::Create( context, TagTypeKind::TTK_Struct, 
    context.getTranslationUnitDecl(), NoLoc, NoLoc, &id, nullptr, DelayTypeCreationTrue);
  decl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));
  decl->startDefinition();
  return decl;
}

void FinishSubobjectDecl(ASTContext& context, CXXRecordDecl *decl) {
  decl->completeDefinition();
  context.getTranslationUnitDecl()->addDecl(decl);
  decl->setImplicit(true);
}

// struct StateObjectConfig 
// {
//   uint32_t Flags;
// };
static CXXRecordDecl *CreateSubobjectStateObjectConfig(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "StateObjectConfig");
  CreateSimpleField(context, decl, "Flags", context.UnsignedIntTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct GlobalRootSignature
// {
//   string signature;
// };
static CXXRecordDecl *CreateSubobjectRootSignature(ASTContext& context, bool global) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, global ? "GlobalRootSignature" : "LocalRootSignature");
  CreateSimpleField(context, decl, "Data", context.HLSLStringTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct SubobjectToExportsAssociation 
// {
//   string Subobject;
//   string Exports;
// };
static CXXRecordDecl *CreateSubobjectSubobjectToExportsAssoc(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "SubobjectToExportsAssociation");
  CreateSimpleField(context, decl, "Subobject", context.HLSLStringTy, AccessSpecifier::AS_private);
  CreateSimpleField(context, decl, "Exports",   context.HLSLStringTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct RaytracingShaderConfig 
// {
//   uint32_t MaxPayloadSizeInBytes;
//   uint32_t MaxAttributeSizeInBytes;
// };
static CXXRecordDecl *CreateSubobjectRaytracingShaderConfig(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "RaytracingShaderConfig");
  CreateSimpleField(context, decl, "MaxPayloadSizeInBytes",   context.UnsignedIntTy, AccessSpecifier::AS_private);
  CreateSimpleField(context, decl, "MaxAttributeSizeInBytes", context.UnsignedIntTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct RaytracingPipelineConfig 
// {
//   uint32_t MaxTraceRecursionDepth;
// };
static CXXRecordDecl *CreateSubobjectRaytracingPipelineConfig(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "RaytracingPipelineConfig"); 
  CreateSimpleField(context, decl, "MaxTraceRecursionDepth", context.UnsignedIntTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct TriangleHitGroup
// {
//   string AnyHit;
//   string ClosestHit;
// };
static CXXRecordDecl *CreateSubobjectTriangleHitGroup(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "TriangleHitGroup");
  CreateSimpleField(context, decl, "AnyHit",       context.HLSLStringTy, AccessSpecifier::AS_private);
  CreateSimpleField(context, decl, "ClosestHit",   context.HLSLStringTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

// struct ProceduralPrimitiveHitGroup
// {
//   string AnyHit;
//   string ClosestHit;
//   string Intersection;
// };
static CXXRecordDecl *CreateSubobjectProceduralPrimitiveHitGroup(ASTContext& context) {
  CXXRecordDecl *decl = StartSubobjectDecl(context, "ProceduralPrimitiveHitGroup");
  CreateSimpleField(context, decl, "AnyHit", context.HLSLStringTy, AccessSpecifier::AS_private);
  CreateSimpleField(context, decl, "ClosestHit", context.HLSLStringTy, AccessSpecifier::AS_private);
  CreateSimpleField(context, decl, "Intersection", context.HLSLStringTy, AccessSpecifier::AS_private);
  FinishSubobjectDecl(context, decl);
  return decl;
}

//
// This is similar to clang/Analysis/CallGraph, but the following differences
// motivate this:
//
// - track traversed vs. observed nodes explicitly
// - fully visit all reachable functions
// - merge graph visiting with checking for recursion
// - track global variables and types used (NYI)
//
namespace hlsl {
  struct CallNode {
    FunctionDecl *CallerFn;
    ::llvm::SmallPtrSet<FunctionDecl *, 4> CalleeFns;
  };
  typedef ::llvm::DenseMap<FunctionDecl*, CallNode> CallNodes;
  typedef ::llvm::SmallPtrSet<Decl *, 8> FnCallStack;
  typedef ::llvm::SmallPtrSet<FunctionDecl*, 128> FunctionSet;
  typedef ::llvm::SmallVector<FunctionDecl*, 32> PendingFunctions;

  // Returns the definition of a function.
  // This serves two purposes - ignore built-in functions, and pick
  // a single Decl * to be used in maps and sets.
  static FunctionDecl *getFunctionWithBody(FunctionDecl *F) {
    if (!F) return nullptr;
    if (F->doesThisDeclarationHaveABody()) return F;
    F = F->getFirstDecl();
    for (auto &&Candidate : F->redecls()) {
      if (Candidate->doesThisDeclarationHaveABody()) {
        return Candidate;
      }
    }
    return nullptr;
  }

  // AST visitor that maintains visited and pending collections, as well
  // as recording nodes of caller/callees.
  class FnReferenceVisitor : public RecursiveASTVisitor<FnReferenceVisitor> {
  private:
    CallNodes &m_callNodes;
    FunctionSet &m_visitedFunctions;
    PendingFunctions &m_pendingFunctions;
    FunctionDecl *m_source;
    CallNodes::iterator m_sourceIt;

  public:
    FnReferenceVisitor(FunctionSet &visitedFunctions,
      PendingFunctions &pendingFunctions, CallNodes &callNodes)
      : m_callNodes(callNodes),
      m_visitedFunctions(visitedFunctions),
      m_pendingFunctions(pendingFunctions) {}

    void setSourceFn(FunctionDecl *F) {
      F = getFunctionWithBody(F);
      m_source = F;
      m_sourceIt = m_callNodes.find(F);
    }

    bool VisitDeclRefExpr(DeclRefExpr *ref) {
      ValueDecl *valueDecl = ref->getDecl();
      FunctionDecl *fnDecl = dyn_cast_or_null<FunctionDecl>(valueDecl);
      fnDecl = getFunctionWithBody(fnDecl);
      if (fnDecl) {
        if (m_sourceIt == m_callNodes.end()) {
          auto result = m_callNodes.insert(
            std::pair<FunctionDecl *, CallNode>(m_source, CallNode{ m_source }));
          DXASSERT(result.second == true,
            "else setSourceFn didn't assign m_sourceIt");
          m_sourceIt = result.first;
        }
        m_sourceIt->second.CalleeFns.insert(fnDecl);
        if (!m_visitedFunctions.count(fnDecl)) {
          m_pendingFunctions.push_back(fnDecl);
        }
      }
      return true;
    }
  };

  // A call graph that can check for reachability and recursion efficiently.
  class CallGraphWithRecurseGuard {
  private:
    CallNodes m_callNodes;
    FunctionSet m_visitedFunctions;

    FunctionDecl *CheckRecursion(FnCallStack &CallStack,
      FunctionDecl *D) const {
      if (CallStack.insert(D).second == false)
        return D;
      auto node = m_callNodes.find(D);
      if (node != m_callNodes.end()) {
        for (FunctionDecl *Callee : node->second.CalleeFns) {
          FunctionDecl *pResult = CheckRecursion(CallStack, Callee);
          if (pResult)
            return pResult;
        }
      }
      CallStack.erase(D);
      return nullptr;
    }

  public:
    void BuildForEntry(FunctionDecl *EntryFnDecl) {
      DXASSERT_NOMSG(EntryFnDecl);
      EntryFnDecl = getFunctionWithBody(EntryFnDecl);
      PendingFunctions pendingFunctions;
      FnReferenceVisitor visitor(m_visitedFunctions, pendingFunctions, m_callNodes);
      pendingFunctions.push_back(EntryFnDecl);
      while (!pendingFunctions.empty()) {
        FunctionDecl *pendingDecl = pendingFunctions.pop_back_val();
        if (m_visitedFunctions.insert(pendingDecl).second == true) {
          visitor.setSourceFn(pendingDecl);
          visitor.TraverseDecl(pendingDecl);
        }
      }
    }

    FunctionDecl *CheckRecursion(FunctionDecl *EntryFnDecl) const {
      FnCallStack CallStack;
      EntryFnDecl = getFunctionWithBody(EntryFnDecl);
      return CheckRecursion(CallStack, EntryFnDecl);
    }

    void dump() const {
      OutputDebugStringW(L"Call Nodes:\r\n");
      for (auto &node : m_callNodes) {
        OutputDebugFormatA("%s [%p]:\r\n", node.first->getName().str().c_str(), (void*)node.first);
        for (auto callee : node.second.CalleeFns) {
          OutputDebugFormatA("    %s [%p]\r\n", callee->getName().str().c_str(), (void*)callee);
        }
      }
    }
  };
}

/// <summary>Creates a Typedef in the specified ASTContext.</summary>
static
TypedefDecl *CreateGlobalTypedef(ASTContext* context, const char* ident, QualType baseType)
{
  DXASSERT_NOMSG(context != nullptr);
  DXASSERT_NOMSG(ident != nullptr);
  DXASSERT_NOMSG(!baseType.isNull());

  DeclContext* declContext = context->getTranslationUnitDecl();
  TypeSourceInfo* typeSource = context->getTrivialTypeSourceInfo(baseType);
  TypedefDecl* decl = TypedefDecl::Create(*context, declContext, NoLoc, NoLoc, &context->Idents.get(ident), typeSource);
  declContext->addDecl(decl);
  decl->setImplicit(true);
  return decl;
}

class HLSLExternalSource : public ExternalSemaSource {
private:
  // Inner types.
  struct FindStructBasicTypeResult {
    ArBasicKind Kind; // Kind of struct (eg, AR_OBJECT_TEXTURE2D)
    unsigned int BasicKindsAsTypeIndex; // Index into g_ArBasicKinds*

    FindStructBasicTypeResult(ArBasicKind kind,
                              unsigned int basicKindAsTypeIndex)
        : Kind(kind), BasicKindsAsTypeIndex(basicKindAsTypeIndex) {}

    bool Found() const { return Kind != AR_BASIC_UNKNOWN; }
  };

  // Declaration for matrix and vector templates.
  ClassTemplateDecl* m_matrixTemplateDecl;
  ClassTemplateDecl* m_vectorTemplateDecl;
  // Namespace decl for hlsl intrin functions
  NamespaceDecl*     m_hlslNSDecl;
  // Context being processed.
  _Notnull_ ASTContext* m_context;

  // Semantic analyzer being processed.
  Sema* m_sema;

  // Intrinsic tables available externally.
  llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2> m_intrinsicTables;

  // Scalar types indexed by HLSLScalarType.
  QualType m_scalarTypes[HLSLScalarTypeCount];

  // Scalar types already built.
  TypedefDecl* m_scalarTypeDefs[HLSLScalarTypeCount];

  // Matrix types already built indexed by type, row-count, col-count. Should probably move to a sparse map. Instrument to figure out best initial size.
  QualType m_matrixTypes[HLSLScalarTypeCount][4][4];

  // Matrix types already built, in shorthand form.
  TypedefDecl* m_matrixShorthandTypes[HLSLScalarTypeCount][4][4];

  // Vector types already built.
  QualType m_vectorTypes[HLSLScalarTypeCount][4];
  TypedefDecl* m_vectorTypedefs[HLSLScalarTypeCount][4];

  // BuiltinType for each scalar type.
  QualType m_baseTypes[HLSLScalarTypeCount];

  // String type
  QualType m_hlslStringType;
  TypedefDecl* m_hlslStringTypedef;

  // Built-in object types declarations, indexed by basic kind constant.
  CXXRecordDecl* m_objectTypeDecls[_countof(g_ArBasicKindsAsTypes)];
  // Map from object decl to the object index.
  using ObjectTypeDeclMapType = std::array<std::pair<CXXRecordDecl*,unsigned>, _countof(g_ArBasicKindsAsTypes)+_countof(g_DeprecatedEffectObjectNames)>;
  ObjectTypeDeclMapType m_objectTypeDeclsMap;
  // Mask for object which not has methods created.
  uint64_t m_objectTypeLazyInitMask;

  UsedIntrinsicStore m_usedIntrinsics;

  /// <summary>Add all base QualTypes for each hlsl scalar types.</summary>
  void AddBaseTypes();

  /// <summary>Adds all supporting declarations to reference scalar types.</summary>
  void AddHLSLScalarTypes();

  /// <summary>Adds string type QualType for HSLS string declarations</summary>
  void AddHLSLStringType();

  QualType GetTemplateObjectDataType(_In_ CXXRecordDecl* recordDecl)
  {
    DXASSERT_NOMSG(recordDecl != nullptr);
    TemplateParameterList* parameterList = recordDecl->getTemplateParameterList(0);
    NamedDecl* parameterDecl = parameterList->getParam(0);

    DXASSERT(parameterDecl->getKind() == Decl::Kind::TemplateTypeParm, "otherwise recordDecl isn't one of the built-in objects with templates");
    TemplateTypeParmDecl* parmDecl = dyn_cast<TemplateTypeParmDecl>(parameterDecl);
    return QualType(parmDecl->getTypeForDecl(), 0);
  }

  // Determines whether the given intrinsic parameter type has a single QualType mapping.
  QualType GetSingleQualTypeForMapping(const HLSL_INTRINSIC* intrinsic, int index)
  {
    int templateRef = intrinsic->pArgs[index].uTemplateId;
    int componentRef = intrinsic->pArgs[index].uComponentTypeId;
    const HLSL_INTRINSIC_ARGUMENT* templateArg = &intrinsic->pArgs[templateRef];
    const HLSL_INTRINSIC_ARGUMENT* componentArg = &intrinsic->pArgs[componentRef];
    const HLSL_INTRINSIC_ARGUMENT* matrixArg = &intrinsic->pArgs[index];

    if (
      templateRef >= 0 &&
      templateArg->uTemplateId == templateRef &&
      !DoesLegalTemplateAcceptMultipleTypes(templateArg->uLegalTemplates) &&
      componentRef >= 0 &&
      componentRef != INTRIN_COMPTYPE_FROM_TYPE_ELT0 &&
      componentArg->uComponentTypeId == 0 &&
      !DoesComponentTypeAcceptMultipleTypes(componentArg->uLegalComponentTypes) &&
      !IsRowOrColumnVariable(matrixArg->uCols) &&
      !IsRowOrColumnVariable(matrixArg->uRows))
    {
      ArTypeObjectKind templateKind = g_LegalIntrinsicTemplates[templateArg->uLegalTemplates][0];
      ArBasicKind elementKind = g_LegalIntrinsicCompTypes[componentArg->uLegalComponentTypes][0];
      return NewSimpleAggregateType(templateKind, elementKind, 0, matrixArg->uRows, matrixArg->uCols);
    }

    return QualType();
  }

  // Adds a new template parameter declaration to the specified array and returns the type for the parameter.
  QualType AddTemplateParamToArray(_In_z_ const char* name, _Inout_ CXXRecordDecl* recordDecl, int templateDepth,
    _Inout_count_c_(g_MaxIntrinsicParamCount + 1) NamedDecl* (&templateParamNamedDecls)[g_MaxIntrinsicParamCount + 1],
    _Inout_ size_t* templateParamNamedDeclsCount)
  {
    DXASSERT_NOMSG(name != nullptr);
    DXASSERT_NOMSG(recordDecl != nullptr);
    DXASSERT_NOMSG(templateParamNamedDecls != nullptr);
    DXASSERT_NOMSG(templateParamNamedDeclsCount != nullptr);
    DXASSERT(*templateParamNamedDeclsCount < _countof(templateParamNamedDecls), "otherwise constants should be updated");
    _Analysis_assume_(*templateParamNamedDeclsCount < _countof(templateParamNamedDecls));

    // Create the declaration for the template parameter.
    IdentifierInfo* id = &m_context->Idents.get(StringRef(name));
    TemplateTypeParmDecl* templateTypeParmDecl =
      TemplateTypeParmDecl::Create(*m_context, recordDecl, NoLoc, NoLoc, templateDepth, *templateParamNamedDeclsCount,
      id, TypenameTrue, ParameterPackFalse);
    templateParamNamedDecls[*templateParamNamedDeclsCount] = templateTypeParmDecl;

    // Create the type that the parameter represents.
    QualType result = m_context->getTemplateTypeParmType(
      templateDepth, *templateParamNamedDeclsCount, ParameterPackFalse, templateTypeParmDecl);

    // Increment the declaration count for the array; as long as caller passes in both arguments,
    // it need not concern itself with maintaining this value.
    (*templateParamNamedDeclsCount)++;

    return result;
  }

  // Adds a function specified by the given intrinsic to a record declaration.
  // The template depth will be zero for records that don't have a "template<>" line
  // even if conceptual; or one if it does have one.
  void AddObjectIntrinsicTemplate(_Inout_ CXXRecordDecl* recordDecl, int templateDepth, _In_ const HLSL_INTRINSIC* intrinsic)
  {
    DXASSERT_NOMSG(recordDecl != nullptr);
    DXASSERT_NOMSG(intrinsic != nullptr);
    DXASSERT(intrinsic->uNumArgs > 0, "otherwise there isn't even an intrinsic name");
    DXASSERT(intrinsic->uNumArgs <= (g_MaxIntrinsicParamCount + 1), "otherwise g_MaxIntrinsicParamCount should be updated");
    
    // uNumArgs includes the result type, g_MaxIntrinsicParamCount doesn't, thus the +1.
    _Analysis_assume_(intrinsic->uNumArgs <= (g_MaxIntrinsicParamCount + 1));

    // TODO: implement template parameter constraints for HLSL intrinsic methods in declarations

    //
    // Build template parameters, parameter types, and the return type.
    // Parameter declarations are built after the function is created, to use it as their scope.
    //
    unsigned int numParams = intrinsic->uNumArgs - 1;
    NamedDecl* templateParamNamedDecls[g_MaxIntrinsicParamCount + 1];
    size_t templateParamNamedDeclsCount = 0;
    QualType argsQTs[g_MaxIntrinsicParamCount];
    StringRef argNames[g_MaxIntrinsicParamCount];
    QualType functionResultQT;

    DXASSERT(
      _countof(templateParamNamedDecls) >= numParams + 1,
      "need enough templates for all parameters and the return type, otherwise constants need updating");

    // Handle the return type.
    // functionResultQT = GetSingleQualTypeForMapping(intrinsic, 0);
    // if (functionResultQT.isNull()) {
    // Workaround for template parameter argument count mismatch.
    // Create template parameter for return type always
    // TODO: reenable the check and skip template argument.
    functionResultQT = AddTemplateParamToArray(
        "TResult", recordDecl, templateDepth, templateParamNamedDecls,
        &templateParamNamedDeclsCount);
    // }

    SmallVector<hlsl::ParameterModifier, g_MaxIntrinsicParamCount> paramMods;
    InitParamMods(intrinsic, paramMods);

    // Consider adding more cases where return type can be handled a priori. Ultimately #260431 should do significantly better.

    // Handle parameters.
    for (unsigned int i = 1; i < intrinsic->uNumArgs; i++)
    {
      //
      // GetSingleQualTypeForMapping can be used here to remove unnecessary template arguments.
      //
      // However this may produce template instantiations with equivalent template arguments
      // for overloaded methods. It's possible to resolve some of these by generating specializations,
      // but the current intrinsic table has rules that are hard to process in their current form
      // to find all cases.
      //
      char name[g_MaxIntrinsicParamName + 2];
      name[0] = 'T';
      name[1] = '\0';
      strcat_s(name, intrinsic->pArgs[i].pName);
      argsQTs[i - 1] = AddTemplateParamToArray(name, recordDecl, templateDepth, templateParamNamedDecls, &templateParamNamedDeclsCount);
      // Change out/inout param to reference type.
      if (paramMods[i-1].isAnyOut()) 
        argsQTs[i - 1] = m_context->getLValueReferenceType(argsQTs[i - 1]);
      
      argNames[i - 1] = StringRef(intrinsic->pArgs[i].pName);
    }

    // Create the declaration.
    IdentifierInfo* ii = &m_context->Idents.get(StringRef(intrinsic->pArgs[0].pName));
    DeclarationName declarationName = DeclarationName(ii);
    CXXMethodDecl* functionDecl = CreateObjectFunctionDeclarationWithParams(*m_context, recordDecl,
      functionResultQT, ArrayRef<QualType>(argsQTs, numParams), ArrayRef<StringRef>(argNames, numParams),
      declarationName, true);
    functionDecl->setImplicit(true);

    // If the function is a template function, create the declaration and cross-reference.
    if (templateParamNamedDeclsCount > 0)
    {
      hlsl::CreateFunctionTemplateDecl(
        *m_context, recordDecl, functionDecl, templateParamNamedDecls, templateParamNamedDeclsCount);
    }
  }

  // Checks whether the two specified intrinsics generate equivalent templates.
  // For example: foo (any_int) and foo (any_float) are only unambiguous in the context
  // of HLSL intrinsic rules, and their difference can't be expressed with C++ templates.
  bool AreIntrinsicTemplatesEquivalent(const HLSL_INTRINSIC* left, const HLSL_INTRINSIC* right)
  {
    if (left == right)
    {
      return true;
    }
    if (left == nullptr || right == nullptr)
    {
      return false;
    }

    return (left->uNumArgs == right->uNumArgs &&
      0 == strcmp(left->pArgs[0].pName, right->pArgs[0].pName));
  }

  // Adds all the intrinsic methods that correspond to the specified type.
  void AddObjectMethods(ArBasicKind kind, _In_ CXXRecordDecl* recordDecl, int templateDepth)
  {
    DXASSERT_NOMSG(recordDecl != nullptr);
    DXASSERT_NOMSG(templateDepth >= 0);

    const HLSL_INTRINSIC* intrinsics;
    const HLSL_INTRINSIC* prior = nullptr;
    size_t intrinsicCount;

    GetIntrinsicMethods(kind, &intrinsics, &intrinsicCount);
    DXASSERT(
      (intrinsics == nullptr) == (intrinsicCount == 0),
      "intrinsic table pointer must match count (null for zero, something valid otherwise");

    while (intrinsicCount--)
    {
      if (!AreIntrinsicTemplatesEquivalent(intrinsics, prior))
      {
        AddObjectIntrinsicTemplate(recordDecl, templateDepth, intrinsics);
        prior = intrinsics;
      }

      intrinsics++;
    }
  }

  void AddDoubleSubscriptSupport(
    _In_ ClassTemplateDecl* typeDecl,
    _In_ CXXRecordDecl* recordDecl,
    _In_z_ const char* memberName, QualType elementType, TemplateTypeParmDecl* templateTypeParmDecl,
    _In_z_ const char* type0Name,
    _In_z_ const char* type1Name,
    _In_z_ const char* indexer0Name, QualType indexer0Type,
    _In_z_ const char* indexer1Name, QualType indexer1Type)
  {
    DXASSERT_NOMSG(typeDecl != nullptr);
    DXASSERT_NOMSG(recordDecl != nullptr);
    DXASSERT_NOMSG(memberName != nullptr);
    DXASSERT_NOMSG(!elementType.isNull());
    DXASSERT_NOMSG(templateTypeParmDecl != nullptr);
    DXASSERT_NOMSG(type0Name != nullptr);
    DXASSERT_NOMSG(type1Name != nullptr);
    DXASSERT_NOMSG(indexer0Name != nullptr);
    DXASSERT_NOMSG(!indexer0Type.isNull());
    DXASSERT_NOMSG(indexer1Name != nullptr);
    DXASSERT_NOMSG(!indexer1Type.isNull());

    //
    // Add inner types to the templates to represent the following C++ code inside the class.
    // public:
    //  class sample_slice_type
    //  {
    //  public: TElement operator[](uint3 index);
    //  };
    //  class sample_type
    //  {
    //  public: sample_slice_type operator[](uint slice);
    //  };
    //  sample_type sample;
    //
    // Variable names reflect this structure, but this code will also produce the types
    // for .mips access.
    //
    const bool MutableTrue = true;
    DeclarationName subscriptName = m_context->DeclarationNames.getCXXOperatorName(OO_Subscript);
    CXXRecordDecl* sampleSliceTypeDecl = CXXRecordDecl::Create(*m_context, TTK_Class, recordDecl, NoLoc, NoLoc,
      &m_context->Idents.get(StringRef(type1Name)));
    sampleSliceTypeDecl->setAccess(AS_public);
    sampleSliceTypeDecl->setImplicit();
    recordDecl->addDecl(sampleSliceTypeDecl);
    sampleSliceTypeDecl->startDefinition();
    const bool MutableFalse = false;
    FieldDecl* sliceHandleDecl = FieldDecl::Create(*m_context, sampleSliceTypeDecl, NoLoc, NoLoc,
      &m_context->Idents.get(StringRef("handle")), indexer0Type,
      m_context->CreateTypeSourceInfo(indexer0Type), nullptr, MutableFalse, ICIS_NoInit);
    sliceHandleDecl->setAccess(AS_private);
    sampleSliceTypeDecl->addDecl(sliceHandleDecl);

    CXXMethodDecl* sampleSliceSubscriptDecl = CreateObjectFunctionDeclarationWithParams(*m_context,
      sampleSliceTypeDecl, elementType,
      ArrayRef<QualType>(indexer1Type), ArrayRef<StringRef>(StringRef(indexer1Name)), subscriptName, true);
    hlsl::CreateFunctionTemplateDecl(*m_context, sampleSliceTypeDecl, sampleSliceSubscriptDecl,
      reinterpret_cast<NamedDecl**>(&templateTypeParmDecl), 1);
    sampleSliceTypeDecl->completeDefinition();

    CXXRecordDecl* sampleTypeDecl = CXXRecordDecl::Create(*m_context, TTK_Class, recordDecl, NoLoc, NoLoc,
      &m_context->Idents.get(StringRef(type0Name)));
    sampleTypeDecl->setAccess(AS_public);
    recordDecl->addDecl(sampleTypeDecl);
    sampleTypeDecl->startDefinition();
    sampleTypeDecl->setImplicit();

    FieldDecl* sampleHandleDecl = FieldDecl::Create(*m_context, sampleTypeDecl, NoLoc, NoLoc,
      &m_context->Idents.get(StringRef("handle")), indexer0Type,
      m_context->CreateTypeSourceInfo(indexer0Type), nullptr, MutableFalse, ICIS_NoInit);
    sampleHandleDecl->setAccess(AS_private);
    sampleTypeDecl->addDecl(sampleHandleDecl);

    QualType sampleSliceType = m_context->getRecordType(sampleSliceTypeDecl);

    CXXMethodDecl* sampleSubscriptDecl = CreateObjectFunctionDeclarationWithParams(*m_context,
      sampleTypeDecl, m_context->getLValueReferenceType(sampleSliceType),
      ArrayRef<QualType>(indexer0Type), ArrayRef<StringRef>(StringRef(indexer0Name)), subscriptName, true);
    sampleTypeDecl->completeDefinition();

    // Add subscript attribute
    AddHLSLSubscriptAttr(sampleSubscriptDecl, *m_context, HLSubscriptOpcode::DoubleSubscript);

    QualType sampleTypeQT = m_context->getRecordType(sampleTypeDecl);
    FieldDecl* sampleFieldDecl = FieldDecl::Create(*m_context, recordDecl, NoLoc, NoLoc,
      &m_context->Idents.get(StringRef(memberName)), sampleTypeQT,
      m_context->CreateTypeSourceInfo(sampleTypeQT), nullptr, MutableTrue, ICIS_NoInit);
    sampleFieldDecl->setAccess(AS_public);
    recordDecl->addDecl(sampleFieldDecl);
  }

  void AddObjectSubscripts(ArBasicKind kind, _In_ ClassTemplateDecl *typeDecl,
                           _In_ CXXRecordDecl *recordDecl,
                           SubscriptOperatorRecord op) {
    DXASSERT_NOMSG(typeDecl != nullptr);
    DXASSERT_NOMSG(recordDecl != nullptr);
    DXASSERT_NOMSG(0 <= op.SubscriptCardinality &&
                   op.SubscriptCardinality <= 3);
    DXASSERT(op.SubscriptCardinality > 0 ||
                 (op.HasMips == false && op.HasSample == false),
             "objects that have .mips or .sample member also have a plain "
             "subscript defined (otherwise static table is "
             "likely incorrect, and this function won't know the cardinality "
             "of the position parameter");

    bool isReadWrite = GetBasicKindProps(kind) & BPROP_RWBUFFER;
    DXASSERT(!isReadWrite || (op.HasMips == false && op.HasSample == false),
             "read/write objects don't have .mips or .sample members");

    // Return early if there is no work to be done.
    if (op.SubscriptCardinality == 0) {
      return;
    }

    const unsigned int templateDepth = 1;

    // Add an operator[].
    TemplateTypeParmDecl *templateTypeParmDecl = cast<TemplateTypeParmDecl>(
        typeDecl->getTemplateParameters()->getParam(0));
    QualType resultType = m_context->getTemplateTypeParmType(
        templateDepth, 0, ParameterPackFalse, templateTypeParmDecl);
    if (!isReadWrite) resultType = m_context->getConstType(resultType);
    resultType = m_context->getLValueReferenceType(resultType);

    QualType indexType =
        op.SubscriptCardinality == 1
            ? m_context->UnsignedIntTy
            : NewSimpleAggregateType(AR_TOBJ_VECTOR, AR_BASIC_UINT32, 0, 1,
                                     op.SubscriptCardinality);

    CXXMethodDecl *functionDecl = CreateObjectFunctionDeclarationWithParams(
        *m_context, recordDecl, resultType, ArrayRef<QualType>(indexType),
        ArrayRef<StringRef>(StringRef("index")),
        m_context->DeclarationNames.getCXXOperatorName(OO_Subscript), true);
    hlsl::CreateFunctionTemplateDecl(
        *m_context, recordDecl, functionDecl,
        reinterpret_cast<NamedDecl **>(&templateTypeParmDecl), 1);

    // Add a .mips member if necessary.
    QualType uintType = m_context->UnsignedIntTy;
    if (op.HasMips) {
      AddDoubleSubscriptSupport(typeDecl, recordDecl, "mips", resultType,
                                templateTypeParmDecl, "mips_type",
                                "mips_slice_type", "mipSlice", uintType, "pos",
                                indexType);
    }

    // Add a .sample member if necessary.
    if (op.HasSample) {
      AddDoubleSubscriptSupport(typeDecl, recordDecl, "sample", resultType,
                                templateTypeParmDecl, "sample_type",
                                "sample_slice_type", "sampleSlice", uintType,
                                "pos", indexType);
      // TODO: support operator[][](indexType, uint).
    }
  }

  static bool ObjectTypeDeclMapTypeCmp(const std::pair<CXXRecordDecl*,unsigned> &a,
                           const std::pair<CXXRecordDecl*,unsigned> &b) {
    return a.first < b.first;
  };

  int FindObjectBasicKindIndex(const CXXRecordDecl* recordDecl) {
    auto begin = m_objectTypeDeclsMap.begin();
    auto end = m_objectTypeDeclsMap.end();
    auto val = std::make_pair(const_cast<CXXRecordDecl*>(recordDecl), 0);
    auto low = std::lower_bound(begin, end, val, ObjectTypeDeclMapTypeCmp);
    if (low == end)
      return -1;
    if (recordDecl == low->first)
      return low->second;
    else
      return -1;
  }

  // Adds all built-in HLSL object types.
  void AddObjectTypes()
  {
    DXASSERT(m_context != nullptr, "otherwise caller hasn't initialized context yet");

    QualType float4Type = LookupVectorType(HLSLScalarType_float, 4);
    TypeSourceInfo *float4TypeSourceInfo = m_context->getTrivialTypeSourceInfo(float4Type, NoLoc);
    m_objectTypeLazyInitMask = 0;
    unsigned effectKindIndex = 0;
    for (unsigned i = 0; i < _countof(g_ArBasicKindsAsTypes); i++)
    {
      ArBasicKind kind = g_ArBasicKindsAsTypes[i];
      if (kind == AR_OBJECT_WAVE) { // wave objects are currently unused
        continue;
      }
      if (kind == AR_OBJECT_LEGACY_EFFECT)
        effectKindIndex = i;

      DXASSERT(kind < _countof(g_ArBasicTypeNames), "g_ArBasicTypeNames has the wrong number of entries");
      _Analysis_assume_(kind < _countof(g_ArBasicTypeNames));
      const char* typeName = g_ArBasicTypeNames[kind];
      uint8_t templateArgCount = g_ArBasicKindsTemplateCount[i];
      CXXRecordDecl* recordDecl = nullptr;
      if (kind == AR_OBJECT_RAY_DESC) {
        QualType float3Ty = LookupVectorType(HLSLScalarType::HLSLScalarType_float, 3);
        recordDecl = CreateRayDescStruct(*m_context, float3Ty);
      } else if (kind == AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES) {
        QualType float2Type = LookupVectorType(HLSLScalarType::HLSLScalarType_float, 2);
        recordDecl = AddBuiltInTriangleIntersectionAttributes(*m_context, float2Type);
      } else if (IsSubobjectBasicKind(kind)) {
        switch (kind) {
        case AR_OBJECT_STATE_OBJECT_CONFIG:
          recordDecl = CreateSubobjectStateObjectConfig(*m_context);
          break;
        case AR_OBJECT_GLOBAL_ROOT_SIGNATURE:
          recordDecl = CreateSubobjectRootSignature(*m_context, true);
          break;
        case AR_OBJECT_LOCAL_ROOT_SIGNATURE:
          recordDecl = CreateSubobjectRootSignature(*m_context, false);
          break;
        case AR_OBJECT_SUBOBJECT_TO_EXPORTS_ASSOC:
          recordDecl = CreateSubobjectSubobjectToExportsAssoc(*m_context);
          break;
          break;
        case AR_OBJECT_RAYTRACING_SHADER_CONFIG:
          recordDecl = CreateSubobjectRaytracingShaderConfig(*m_context);
          break;
        case AR_OBJECT_RAYTRACING_PIPELINE_CONFIG:
          recordDecl = CreateSubobjectRaytracingPipelineConfig(*m_context);
          break;
        case AR_OBJECT_TRIANGLE_HIT_GROUP:
          recordDecl = CreateSubobjectTriangleHitGroup(*m_context);
          break;
        case AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP:
          recordDecl = CreateSubobjectProceduralPrimitiveHitGroup(*m_context);
          break;
        }
      }
      else if (templateArgCount == 0)
      {
        AddRecordTypeWithHandle(*m_context, &recordDecl, typeName);
        DXASSERT(recordDecl != nullptr, "AddRecordTypeWithHandle failed to return the object declaration");
        recordDecl->setImplicit(true);
      }
      else
      {
        DXASSERT(templateArgCount == 1 || templateArgCount == 2, "otherwise a new case has been added");

        ClassTemplateDecl* typeDecl = nullptr;
        TypeSourceInfo* typeDefault = TemplateHasDefaultType(kind) ? float4TypeSourceInfo : nullptr;
        AddTemplateTypeWithHandle(*m_context, &typeDecl, &recordDecl, typeName, templateArgCount, typeDefault);
        DXASSERT(typeDecl != nullptr, "AddTemplateTypeWithHandle failed to return the object declaration");
        typeDecl->setImplicit(true);
        recordDecl->setImplicit(true);
      }
      m_objectTypeDecls[i] = recordDecl;
      m_objectTypeDeclsMap[i] = std::make_pair(recordDecl, i);
      m_objectTypeLazyInitMask |= ((uint64_t)1)<<i;
    }

    // Create an alias for SamplerState. 'sampler' is very commonly used.
    {
      DeclContext* currentDeclContext = m_context->getTranslationUnitDecl();
      IdentifierInfo& samplerId = m_context->Idents.get(StringRef("sampler"), tok::TokenKind::identifier);
      TypeSourceInfo* samplerTypeSource = m_context->getTrivialTypeSourceInfo(GetBasicKindType(AR_OBJECT_SAMPLER));
      TypedefDecl* samplerDecl = TypedefDecl::Create(*m_context, currentDeclContext, NoLoc, NoLoc, &samplerId, samplerTypeSource);
      currentDeclContext->addDecl(samplerDecl);
      samplerDecl->setImplicit(true);

      // Create decls for each deprecated effect object type:
      unsigned effectObjBase = _countof(g_ArBasicKindsAsTypes);
      // TypeSourceInfo* effectObjTypeSource = m_context->getTrivialTypeSourceInfo(GetBasicKindType(AR_OBJECT_LEGACY_EFFECT));
      for (unsigned i = 0; i < _countof(g_DeprecatedEffectObjectNames); i++) {
        IdentifierInfo& idInfo = m_context->Idents.get(StringRef(g_DeprecatedEffectObjectNames[i]), tok::TokenKind::identifier);
        //TypedefDecl* effectObjDecl = TypedefDecl::Create(*m_context, currentDeclContext, NoLoc, NoLoc, &idInfo, effectObjTypeSource);
        CXXRecordDecl *effectObjDecl = CXXRecordDecl::Create(*m_context, TagTypeKind::TTK_Struct, currentDeclContext, NoLoc, NoLoc, &idInfo);
        currentDeclContext->addDecl(effectObjDecl);
        effectObjDecl->setImplicit(true);
        m_objectTypeDeclsMap[i+effectObjBase] = std::make_pair(effectObjDecl, effectKindIndex);
      }
    }

    // Make sure it's in order.
    std::sort(m_objectTypeDeclsMap.begin(), m_objectTypeDeclsMap.end(), ObjectTypeDeclMapTypeCmp);
  }

  FunctionDecl* AddSubscriptSpecialization(
    _In_ FunctionTemplateDecl* functionTemplate,
    QualType objectElement,
    const FindStructBasicTypeResult& findResult);

  ImplicitCastExpr* CreateLValueToRValueCast(Expr* input) {
    return ImplicitCastExpr::Create(*m_context, input->getType(), CK_LValueToRValue, input, nullptr, VK_RValue);
  }
  ImplicitCastExpr* CreateFlatConversionCast(Expr* input) {
    return ImplicitCastExpr::Create(*m_context, input->getType(), CK_LValueToRValue, input, nullptr, VK_RValue);
  }

  static TYPE_CONVERSION_REMARKS RemarksUnused;
  static ImplicitConversionKind ImplicitConversionKindUnused;
  HRESULT CombineDimensions(QualType leftType, QualType rightType, QualType *resultType,
                            ImplicitConversionKind &convKind = ImplicitConversionKindUnused,
                            TYPE_CONVERSION_REMARKS &Remarks = RemarksUnused);

  clang::TypedefDecl *LookupMatrixShorthandType(HLSLScalarType scalarType, UINT rowCount, UINT colCount) {
    DXASSERT_NOMSG(scalarType != HLSLScalarType::HLSLScalarType_unknown &&
                   rowCount <= 4 && colCount <= 4);
    TypedefDecl *qts =
        m_matrixShorthandTypes[scalarType][rowCount - 1][colCount - 1];
    if (qts == nullptr) {
      QualType type = LookupMatrixType(scalarType, rowCount, colCount);
      qts = CreateMatrixSpecializationShorthand(*m_context, type, scalarType,
                                                rowCount, colCount);
      m_matrixShorthandTypes[scalarType][rowCount - 1][colCount - 1] = qts;
    }
    return qts;
  }

  clang::TypedefDecl *LookupVectorShorthandType(HLSLScalarType scalarType, UINT colCount) {
    DXASSERT_NOMSG(scalarType != HLSLScalarType::HLSLScalarType_unknown &&
                   colCount <= 4);
    TypedefDecl *qts = m_vectorTypedefs[scalarType][colCount - 1];
    if (qts == nullptr) {
      QualType type = LookupVectorType(scalarType, colCount);
      qts = CreateVectorSpecializationShorthand(*m_context, type, scalarType,
                                                colCount);
      m_vectorTypedefs[scalarType][colCount - 1] = qts;
    }
    return qts;
  }

public:
  HLSLExternalSource() :
    m_matrixTemplateDecl(nullptr),
    m_vectorTemplateDecl(nullptr),
    m_context(nullptr),
    m_sema(nullptr),
    m_hlslStringTypedef(nullptr)
  {
    memset(m_matrixTypes, 0, sizeof(m_matrixTypes));
    memset(m_matrixShorthandTypes, 0, sizeof(m_matrixShorthandTypes));
    memset(m_vectorTypes, 0, sizeof(m_vectorTypes));
    memset(m_vectorTypedefs, 0, sizeof(m_vectorTypedefs));
    memset(m_scalarTypes, 0, sizeof(m_scalarTypes));
    memset(m_scalarTypeDefs, 0, sizeof(m_scalarTypeDefs));
    memset(m_baseTypes, 0, sizeof(m_baseTypes));
  }

  ~HLSLExternalSource() { }

  static HLSLExternalSource* FromSema(_In_ Sema* self)
  {
    DXASSERT_NOMSG(self != nullptr);

    ExternalSemaSource* externalSource = self->getExternalSource();
    DXASSERT(externalSource != nullptr, "otherwise caller shouldn't call HLSL-specific function");

    HLSLExternalSource* hlsl = reinterpret_cast<HLSLExternalSource*>(externalSource);
    return hlsl;
  }

  void InitializeSema(Sema& S) override
  {
    m_sema = &S;
    S.addExternalSource(this);

    AddObjectTypes();
    AddStdIsEqualImplementation(S.getASTContext(), S);
    for (auto && intrinsic : m_intrinsicTables) {
      AddIntrinsicTableMethods(intrinsic);
    }
  }

  void ForgetSema() override
  {
    m_sema = nullptr;
  }

  Sema* getSema() {
    return m_sema;
  }

  TypedefDecl* LookupScalarTypeDef(HLSLScalarType scalarType) {
    // We shouldn't create Typedef for built in scalar types.
    // For built in scalar types, this funciton may be called for
    // TypoCorrection. In that case, we return a nullptr.
    if (m_scalarTypes[scalarType].isNull()) {
      m_scalarTypeDefs[scalarType] = CreateGlobalTypedef(m_context, HLSLScalarTypeNames[scalarType], m_baseTypes[scalarType]);
      m_scalarTypes[scalarType] = m_context->getTypeDeclType(m_scalarTypeDefs[scalarType]);
    }
    return m_scalarTypeDefs[scalarType];
  }

  QualType LookupMatrixType(HLSLScalarType scalarType, unsigned int rowCount, unsigned int colCount)
  {
    QualType qt = m_matrixTypes[scalarType][rowCount - 1][colCount - 1];
    if (qt.isNull()) {
      // lazy initialization of scalar types 
      if (m_scalarTypes[scalarType].isNull()) {
        LookupScalarTypeDef(scalarType);
      }
      qt = GetOrCreateMatrixSpecialization(*m_context, m_sema, m_matrixTemplateDecl, m_scalarTypes[scalarType], rowCount, colCount);
      m_matrixTypes[scalarType][rowCount - 1][colCount - 1] = qt;
    }
    return qt;
  }

  QualType LookupVectorType(HLSLScalarType scalarType, unsigned int colCount)
  {
    QualType qt = m_vectorTypes[scalarType][colCount - 1];
    if (qt.isNull()) {
      if (m_scalarTypes[scalarType].isNull()) {
        LookupScalarTypeDef(scalarType);
      }
      qt = GetOrCreateVectorSpecialization(*m_context, m_sema, m_vectorTemplateDecl, m_scalarTypes[scalarType], colCount);
      m_vectorTypes[scalarType][colCount - 1] = qt;
    }
    return qt;
  }

  TypedefDecl* GetStringTypedef() {
    if (m_hlslStringTypedef == nullptr) {
      m_hlslStringTypedef = CreateGlobalTypedef(m_context, "string", m_hlslStringType);
      m_hlslStringType = m_context->getTypeDeclType(m_hlslStringTypedef);
    }
    DXASSERT_NOMSG(m_hlslStringTypedef != nullptr);
    return m_hlslStringTypedef;
  }

  static bool IsSubobjectBasicKind(ArBasicKind kind) {
    return kind >= AR_OBJECT_STATE_OBJECT_CONFIG && kind <= AR_OBJECT_PROCEDURAL_PRIMITIVE_HIT_GROUP;
  }

  bool IsSubobjectType(QualType type) {
    return IsSubobjectBasicKind(GetTypeElementKind(type));
  }

  void WarnMinPrecision(HLSLScalarType type, SourceLocation loc) {
    // TODO: enalbe this once we introduce precise master option
    bool UseMinPrecision = m_context->getLangOpts().UseMinPrecision;
    if (type == HLSLScalarType_int_min12) {
      const char *PromotedType =
          UseMinPrecision ? HLSLScalarTypeNames[HLSLScalarType_int_min16]
                          : HLSLScalarTypeNames[HLSLScalarType_int16];
      m_sema->Diag(loc, diag::warn_hlsl_sema_minprecision_promotion)
          << HLSLScalarTypeNames[type] << PromotedType;
    } else if (type == HLSLScalarType_float_min10) {
      const char *PromotedType =
          UseMinPrecision ? HLSLScalarTypeNames[HLSLScalarType_float_min16]
                          : HLSLScalarTypeNames[HLSLScalarType_float16];
      m_sema->Diag(loc, diag::warn_hlsl_sema_minprecision_promotion)
          << HLSLScalarTypeNames[type] << PromotedType;
    }
    if (!UseMinPrecision) {
      if (type == HLSLScalarType_float_min16) {
        m_sema->Diag(loc, diag::warn_hlsl_sema_minprecision_promotion)
            << HLSLScalarTypeNames[type]
            << HLSLScalarTypeNames[HLSLScalarType_float16];
      } else if (type == HLSLScalarType_int_min16) {
        m_sema->Diag(loc, diag::warn_hlsl_sema_minprecision_promotion)
            << HLSLScalarTypeNames[type]
            << HLSLScalarTypeNames[HLSLScalarType_int16];
      } else if (type == HLSLScalarType_uint_min16) {
        m_sema->Diag(loc, diag::warn_hlsl_sema_minprecision_promotion)
            << HLSLScalarTypeNames[type]
            << HLSLScalarTypeNames[HLSLScalarType_uint16];
      }
    }
  }

  bool DiagnoseHLSLScalarType(HLSLScalarType type, SourceLocation Loc) {
    if (getSema()->getLangOpts().HLSLVersion < 2018) {
      switch (type) {
      case HLSLScalarType_float16:
      case HLSLScalarType_float32:
      case HLSLScalarType_float64:
      case HLSLScalarType_int16:
      case HLSLScalarType_int32:
      case HLSLScalarType_uint16:
      case HLSLScalarType_uint32:
        m_sema->Diag(Loc, diag::err_hlsl_unsupported_keyword_for_version)
            << HLSLScalarTypeNames[type] << "2018";
        return false;
      default:
        break;
      }
    }
    if (getSema()->getLangOpts().UseMinPrecision) {
      switch (type) {
      case HLSLScalarType_float16:
      case HLSLScalarType_int16:
      case HLSLScalarType_uint16:
        m_sema->Diag(Loc, diag::err_hlsl_unsupported_keyword_for_min_precision)
            << HLSLScalarTypeNames[type];
        return false;
      default:
        break;
      }
    }
    return true;
  }

  bool LookupUnqualified(LookupResult &R, Scope *S) override
  {
    const DeclarationNameInfo declName = R.getLookupNameInfo();
    IdentifierInfo* idInfo = declName.getName().getAsIdentifierInfo();
    if (idInfo == nullptr) {
      return false;
    }

    // Currently template instantiation is blocked when a fatal error is
    // detected. So no faulting-in types at this point, instead we simply
    // back out.
    if (this->m_sema->Diags.hasFatalErrorOccurred()) {
      return false;
    }

    StringRef nameIdentifier = idInfo->getName();
    HLSLScalarType parsedType;
    int rowCount;
    int colCount;

    // Try parsing hlsl scalar types that is not initialized at AST time.
    if (TryParseAny(nameIdentifier.data(), nameIdentifier.size(), &parsedType, &rowCount, &colCount, getSema()->getLangOpts())) {
      assert(parsedType != HLSLScalarType_unknown && "otherwise, TryParseHLSLScalarType should not have succeeded.");
      if (rowCount == 0 && colCount == 0) { // scalar
        TypedefDecl *typeDecl = LookupScalarTypeDef(parsedType);
        if (!typeDecl) return false;
        R.addDecl(typeDecl);
      }
      else if (rowCount == 0) { // vector
        TypedefDecl *qts = LookupVectorShorthandType(parsedType, colCount);
        R.addDecl(qts);
      }
      else { // matrix
        TypedefDecl* qts = LookupMatrixShorthandType(parsedType, rowCount, colCount);
        R.addDecl(qts);
      }
      return true;
    }
    // string
    else if (TryParseString(nameIdentifier.data(), nameIdentifier.size(), getSema()->getLangOpts())) {
      TypedefDecl *strDecl = GetStringTypedef();
      R.addDecl(strDecl);
    }
    return false;
  }

  /// <summary>
  /// Determines whether the specify record type is a matrix, another HLSL object, or a user-defined structure.
  /// </sumary>
  ArTypeObjectKind ClassifyRecordType(const RecordType* type)
  {
    DXASSERT_NOMSG(type != nullptr);

    const CXXRecordDecl* typeRecordDecl = type->getAsCXXRecordDecl();
    const ClassTemplateSpecializationDecl* templateSpecializationDecl = dyn_cast<ClassTemplateSpecializationDecl>(typeRecordDecl);
    if (templateSpecializationDecl) {
      ClassTemplateDecl *decl = templateSpecializationDecl->getSpecializedTemplate();
      if (decl == m_matrixTemplateDecl)
        return AR_TOBJ_MATRIX;
      else if (decl == m_vectorTemplateDecl)
        return AR_TOBJ_VECTOR;
      DXASSERT(decl->isImplicit(), "otherwise object template decl is not set to implicit");
      return AR_TOBJ_OBJECT;
    }

    if (typeRecordDecl && typeRecordDecl->isImplicit()) {
      if (typeRecordDecl->getDeclContext()->isFileContext()) {
        int index = FindObjectBasicKindIndex(typeRecordDecl);
        if (index != -1) {
          ArBasicKind kind  = g_ArBasicKindsAsTypes[index];
          if ( AR_OBJECT_RAY_DESC == kind || AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES == kind)
            return AR_TOBJ_COMPOUND;
        }
        return AR_TOBJ_OBJECT;
      }
      else
        return AR_TOBJ_INNER_OBJ;
    }

    return AR_TOBJ_COMPOUND;
  }

  /// <summary>Given a Clang type, determines whether it is a built-in object type (sampler, texture, etc).</summary>
  bool IsBuiltInObjectType(QualType type)
  {
    type = GetStructuralForm(type);

    if (!type.isNull() && type->isStructureOrClassType()) {
      const RecordType* recordType = type->getAs<RecordType>();
      return ClassifyRecordType(recordType) == AR_TOBJ_OBJECT;
    }

    return false;
  }

  /// <summary>
  /// Given the specified type (typed a DeclContext for convenience), determines its RecordDecl,
  /// possibly refering to original template record if it's a specialization; this makes the result
  /// suitable for looking up in initialization tables.
  /// </summary>
  const CXXRecordDecl* GetRecordDeclForBuiltInOrStruct(const DeclContext* context)
  {
    const CXXRecordDecl* recordDecl;
    if (const ClassTemplateSpecializationDecl* decl = dyn_cast<ClassTemplateSpecializationDecl>(context))
    {
      recordDecl = decl->getSpecializedTemplate()->getTemplatedDecl();
    }
    else
    {
      recordDecl = dyn_cast<CXXRecordDecl>(context);
    }

    return recordDecl;
  }

  /// <summary>Given a Clang type, return the ArTypeObjectKind classification, (eg AR_TOBJ_VECTOR).</summary>
  ArTypeObjectKind GetTypeObjectKind(QualType type)
  {
    DXASSERT_NOMSG(!type.isNull());

    type = GetStructuralForm(type);

    if (type->isVoidType()) return AR_TOBJ_VOID;
    if (type->isArrayType()) {
      return hlsl::IsArrayConstantStringType(type) ? AR_TOBJ_STRING : AR_TOBJ_ARRAY;
    }
    if (type->isPointerType()) {
      return hlsl::IsPointerStringType(type) ? AR_TOBJ_STRING : AR_TOBJ_POINTER;
    }
    if (type->isStructureOrClassType()) {
      const RecordType* recordType = type->getAs<RecordType>();
      return ClassifyRecordType(recordType);
    } else if (const InjectedClassNameType *ClassNameTy =
                   type->getAs<InjectedClassNameType>()) {
      const CXXRecordDecl *typeRecordDecl = ClassNameTy->getDecl();
      const ClassTemplateSpecializationDecl *templateSpecializationDecl =
          dyn_cast<ClassTemplateSpecializationDecl>(typeRecordDecl);
      if (templateSpecializationDecl) {
        ClassTemplateDecl *decl =
            templateSpecializationDecl->getSpecializedTemplate();
        if (decl == m_matrixTemplateDecl)
          return AR_TOBJ_MATRIX;
        else if (decl == m_vectorTemplateDecl)
          return AR_TOBJ_VECTOR;
        DXASSERT(decl->isImplicit(),
                 "otherwise object template decl is not set to implicit");
        return AR_TOBJ_OBJECT;
      }

      if (typeRecordDecl && typeRecordDecl->isImplicit()) {
        if (typeRecordDecl->getDeclContext()->isFileContext())
          return AR_TOBJ_OBJECT;
        else
          return AR_TOBJ_INNER_OBJ;
      }

      return AR_TOBJ_COMPOUND;
    }

    if (type->isBuiltinType()) return AR_TOBJ_BASIC;
    if (type->isEnumeralType()) return AR_TOBJ_BASIC;

    return AR_TOBJ_INVALID;
  }

  /// <summary>Gets the element type of a matrix or vector type (eg, the 'float' in 'float4x4' or 'float4').</summary>
  QualType GetMatrixOrVectorElementType(QualType type)
  {
    type = GetStructuralForm(type);

    const CXXRecordDecl* typeRecordDecl = type->getAsCXXRecordDecl();
    DXASSERT_NOMSG(typeRecordDecl);
    const ClassTemplateSpecializationDecl* templateSpecializationDecl = dyn_cast<ClassTemplateSpecializationDecl>(typeRecordDecl);
    DXASSERT_NOMSG(templateSpecializationDecl);
    DXASSERT_NOMSG(templateSpecializationDecl->getSpecializedTemplate() == m_matrixTemplateDecl ||
      templateSpecializationDecl->getSpecializedTemplate() == m_vectorTemplateDecl);
    return templateSpecializationDecl->getTemplateArgs().get(0).getAsType();
  }

  /// <summary>Gets the type with structural information (elements and shape) for the given type.</summary>
  /// <remarks>This function will strip lvalue/rvalue references, attributes and qualifiers.</remarks>
  QualType GetStructuralForm(QualType type)
  {
    if (type.isNull()) {
      return type;
    }

    const ReferenceType *RefType = nullptr;
    const AttributedType *AttrType = nullptr;
    while ( (RefType = dyn_cast<ReferenceType>(type)) ||
            (AttrType = dyn_cast<AttributedType>(type)))
    {
      type = RefType ? RefType->getPointeeType() : AttrType->getEquivalentType();
    }

    // Despite its name, getCanonicalTypeUnqualified will preserve const for array elements or something
    return QualType(type->getCanonicalTypeUnqualified()->getTypePtr(), 0);
  }

  /// <summary>Given a Clang type, return the ArBasicKind classification for its contents.</summary>
  ArBasicKind GetTypeElementKind(QualType type)
  {
    type = GetStructuralForm(type);

    ArTypeObjectKind kind = GetTypeObjectKind(type);
    if (kind == AR_TOBJ_MATRIX || kind == AR_TOBJ_VECTOR) {
      QualType elementType = GetMatrixOrVectorElementType(type);
      return GetTypeElementKind(elementType);
    }

    if (kind == AR_TOBJ_STRING) {
      return type->isArrayType() ? AR_OBJECT_STRING_LITERAL : AR_OBJECT_STRING;
    }

    if (type->isArrayType()) {
      const ArrayType* arrayType = type->getAsArrayTypeUnsafe();
      return GetTypeElementKind(arrayType->getElementType());
    }

    if (kind == AR_TOBJ_INNER_OBJ) {
      return AR_OBJECT_INNER;
    } else if (kind == AR_TOBJ_OBJECT) {
      // Classify the object as the element type.
      const CXXRecordDecl* typeRecordDecl = GetRecordDeclForBuiltInOrStruct(type->getAsCXXRecordDecl());
      int index = FindObjectBasicKindIndex(typeRecordDecl);
      // NOTE: this will likely need to be updated for specialized records
      DXASSERT(index != -1, "otherwise can't find type we already determined was an object");
      return g_ArBasicKindsAsTypes[index];
    }

    CanQualType canType = type->getCanonicalTypeUnqualified();
    return BasicTypeForScalarType(canType);
  }

  ArBasicKind BasicTypeForScalarType(CanQualType type)
  {
    if (const BuiltinType *BT = dyn_cast<BuiltinType>(type))
    {
      switch (BT->getKind())
      {
      case BuiltinType::Bool: return AR_BASIC_BOOL;
      case BuiltinType::Double: return AR_BASIC_FLOAT64;
      case BuiltinType::Float: return AR_BASIC_FLOAT32;
      case BuiltinType::Half: return AR_BASIC_FLOAT16;
      case BuiltinType::HalfFloat: return AR_BASIC_FLOAT32_PARTIAL_PRECISION;
      case BuiltinType::Int: return AR_BASIC_INT32;
      case BuiltinType::UInt: return AR_BASIC_UINT32;
      case BuiltinType::Short: return AR_BASIC_INT16;
      case BuiltinType::UShort: return AR_BASIC_UINT16;
      case BuiltinType::Long: return AR_BASIC_INT32;
      case BuiltinType::ULong: return AR_BASIC_UINT32;
      case BuiltinType::LongLong: return AR_BASIC_INT64;
      case BuiltinType::ULongLong: return AR_BASIC_UINT64;
      case BuiltinType::Min12Int: return AR_BASIC_MIN12INT;
      case BuiltinType::Min16Float: return AR_BASIC_MIN16FLOAT;
      case BuiltinType::Min16Int: return AR_BASIC_MIN16INT;
      case BuiltinType::Min16UInt: return AR_BASIC_MIN16UINT;
      case BuiltinType::Min10Float: return AR_BASIC_MIN10FLOAT;
      case BuiltinType::LitFloat: return AR_BASIC_LITERAL_FLOAT;
      case BuiltinType::LitInt: return AR_BASIC_LITERAL_INT;
      default:
        // Only builtin types that have basickind equivalents.
        break;
      }
    }
    if (const EnumType *ET = dyn_cast<EnumType>(type)) {
        if (ET->getDecl()->isScopedUsingClassTag())
            return AR_BASIC_ENUM_CLASS;
        return AR_BASIC_ENUM;
    }
    return AR_BASIC_UNKNOWN;
  }

  void AddIntrinsicTableMethods(_In_ IDxcIntrinsicTable *table) {
    DXASSERT_NOMSG(table != nullptr);

    // Function intrinsics are added on-demand, objects get template methods.
    for (unsigned i = 0; i < _countof(g_ArBasicKindsAsTypes); i++) {
      // Grab information already processed by AddObjectTypes.
      ArBasicKind kind = g_ArBasicKindsAsTypes[i];
      const char *typeName = g_ArBasicTypeNames[kind];
      uint8_t templateArgCount = g_ArBasicKindsTemplateCount[i];
      DXASSERT(templateArgCount <= 2, "otherwise a new case has been added");
      int startDepth = (templateArgCount == 0) ? 0 : 1;
      CXXRecordDecl *recordDecl = m_objectTypeDecls[i];
      if (recordDecl == nullptr) {
        DXASSERT(kind == AR_OBJECT_WAVE, "else objects other than reserved not initialized");
        continue;
      }

      // This is a variation of AddObjectMethods using the new table.
      const HLSL_INTRINSIC *pIntrinsic = nullptr;
      const HLSL_INTRINSIC *pPrior = nullptr;
      UINT64 lookupCookie = 0;
      CA2W wideTypeName(typeName);
      HRESULT found = table->LookupIntrinsic(wideTypeName, L"*", &pIntrinsic, &lookupCookie);
      while (pIntrinsic != nullptr && SUCCEEDED(found)) {
        if (!AreIntrinsicTemplatesEquivalent(pIntrinsic, pPrior)) {
          AddObjectIntrinsicTemplate(recordDecl, startDepth, pIntrinsic);
          // NOTE: this only works with the current implementation because
          // intrinsics are alive as long as the table is alive.
          pPrior = pIntrinsic;
        }
        found = table->LookupIntrinsic(wideTypeName, L"*", &pIntrinsic, &lookupCookie);
      }
    }
  }

  void RegisterIntrinsicTable(_In_ IDxcIntrinsicTable *table) {
    DXASSERT_NOMSG(table != nullptr);
    m_intrinsicTables.push_back(table);
    // If already initialized, add methods immediately.
    if (m_sema != nullptr) {
      AddIntrinsicTableMethods(table);
    }
  }

  HLSLScalarType ScalarTypeForBasic(ArBasicKind kind)
  {
    DXASSERT(kind < AR_BASIC_COUNT, "otherwise caller didn't check that the value was in range");
    switch (kind) {
    case AR_BASIC_BOOL:           return HLSLScalarType_bool;
    case AR_BASIC_LITERAL_FLOAT:  return HLSLScalarType_float_lit;
    case AR_BASIC_FLOAT16:        return HLSLScalarType_half;
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
                                  return HLSLScalarType_float;
    case AR_BASIC_FLOAT32:        return HLSLScalarType_float;
    case AR_BASIC_FLOAT64:        return HLSLScalarType_double;
    case AR_BASIC_LITERAL_INT:    return HLSLScalarType_int_lit;
    case AR_BASIC_INT8:           return HLSLScalarType_int;
    case AR_BASIC_UINT8:          return HLSLScalarType_uint;
    case AR_BASIC_INT16:          return HLSLScalarType_int16;
    case AR_BASIC_UINT16:         return HLSLScalarType_uint16;
    case AR_BASIC_INT32:          return HLSLScalarType_int;
    case AR_BASIC_UINT32:         return HLSLScalarType_uint;
    case AR_BASIC_MIN10FLOAT:     return HLSLScalarType_float_min10;
    case AR_BASIC_MIN16FLOAT:     return HLSLScalarType_float_min16;
    case AR_BASIC_MIN12INT:       return HLSLScalarType_int_min12;
    case AR_BASIC_MIN16INT:       return HLSLScalarType_int_min16;
    case AR_BASIC_MIN16UINT:      return HLSLScalarType_uint_min16;

    case AR_BASIC_INT64:          return HLSLScalarType_int64;
    case AR_BASIC_UINT64:         return HLSLScalarType_uint64;
    case AR_BASIC_ENUM:           return HLSLScalarType_int;
    default:
      return HLSLScalarType_unknown;
    }
  }

  QualType GetBasicKindType(ArBasicKind kind)
  {
    DXASSERT_VALIDBASICKIND(kind);

    switch (kind) {
    case AR_OBJECT_NULL:          return m_context->VoidTy;
    case AR_BASIC_BOOL:           return m_context->BoolTy;
    case AR_BASIC_LITERAL_FLOAT:  return m_context->LitFloatTy;
    case AR_BASIC_FLOAT16:        return m_context->HalfTy;
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION: return m_context->HalfFloatTy;
    case AR_BASIC_FLOAT32:        return m_context->FloatTy;
    case AR_BASIC_FLOAT64:        return m_context->DoubleTy;
    case AR_BASIC_LITERAL_INT:    return m_context->LitIntTy;
    case AR_BASIC_INT8:           return m_context->IntTy;
    case AR_BASIC_UINT8:          return m_context->UnsignedIntTy;
    case AR_BASIC_INT16:          return m_context->ShortTy;
    case AR_BASIC_UINT16:         return m_context->UnsignedShortTy;
    case AR_BASIC_INT32:          return m_context->IntTy;
    case AR_BASIC_UINT32:         return m_context->UnsignedIntTy;
    case AR_BASIC_INT64:          return m_context->LongLongTy;
    case AR_BASIC_UINT64:         return m_context->UnsignedLongLongTy;
    case AR_BASIC_MIN10FLOAT:     return m_scalarTypes[HLSLScalarType_float_min10];
    case AR_BASIC_MIN16FLOAT:     return m_scalarTypes[HLSLScalarType_float_min16];
    case AR_BASIC_MIN12INT:       return m_scalarTypes[HLSLScalarType_int_min12];
    case AR_BASIC_MIN16INT:       return m_scalarTypes[HLSLScalarType_int_min16];
    case AR_BASIC_MIN16UINT:      return m_scalarTypes[HLSLScalarType_uint_min16];
    case AR_BASIC_ENUM:           return m_context->IntTy;
    case AR_BASIC_ENUM_CLASS:     return m_context->IntTy;

    case AR_OBJECT_STRING:        return m_hlslStringType;
    
    case AR_OBJECT_LEGACY_EFFECT:   // used for all legacy effect object types

    case AR_OBJECT_TEXTURE1D:
    case AR_OBJECT_TEXTURE1D_ARRAY:
    case AR_OBJECT_TEXTURE2D:
    case AR_OBJECT_TEXTURE2D_ARRAY:
    case AR_OBJECT_TEXTURE3D:
    case AR_OBJECT_TEXTURECUBE:
    case AR_OBJECT_TEXTURECUBE_ARRAY:
    case AR_OBJECT_TEXTURE2DMS:
    case AR_OBJECT_TEXTURE2DMS_ARRAY:

    case AR_OBJECT_SAMPLER:
    case AR_OBJECT_SAMPLERCOMPARISON:

    case AR_OBJECT_BUFFER:

    case AR_OBJECT_POINTSTREAM:
    case AR_OBJECT_LINESTREAM:
    case AR_OBJECT_TRIANGLESTREAM:

    case AR_OBJECT_INPUTPATCH:
    case AR_OBJECT_OUTPUTPATCH:

    case AR_OBJECT_RWTEXTURE1D:
    case AR_OBJECT_RWTEXTURE1D_ARRAY:
    case AR_OBJECT_RWTEXTURE2D:
    case AR_OBJECT_RWTEXTURE2D_ARRAY:
    case AR_OBJECT_RWTEXTURE3D:
    case AR_OBJECT_RWBUFFER:

    case AR_OBJECT_BYTEADDRESS_BUFFER:
    case AR_OBJECT_RWBYTEADDRESS_BUFFER:
    case AR_OBJECT_STRUCTURED_BUFFER:
    case AR_OBJECT_RWSTRUCTURED_BUFFER:
    case AR_OBJECT_APPEND_STRUCTURED_BUFFER:
    case AR_OBJECT_CONSUME_STRUCTURED_BUFFER:
    case AR_OBJECT_WAVE:
    case AR_OBJECT_ACCELERATION_STRUCT:
    case AR_OBJECT_RAY_DESC:
    case AR_OBJECT_TRIANGLE_INTERSECTION_ATTRIBUTES:
    {
        const ArBasicKind* match = std::find(g_ArBasicKindsAsTypes, &g_ArBasicKindsAsTypes[_countof(g_ArBasicKindsAsTypes)], kind);
        DXASSERT(match != &g_ArBasicKindsAsTypes[_countof(g_ArBasicKindsAsTypes)], "otherwise can't find constant in basic kinds");
        size_t index = match - g_ArBasicKindsAsTypes;
        return m_context->getTagDeclType(this->m_objectTypeDecls[index]);
    }

    case AR_OBJECT_SAMPLER1D:
    case AR_OBJECT_SAMPLER2D:
    case AR_OBJECT_SAMPLER3D:
    case AR_OBJECT_SAMPLERCUBE:
      // Turn dimension-typed samplers into sampler states.
      return GetBasicKindType(AR_OBJECT_SAMPLER);

    case AR_OBJECT_STATEBLOCK:

    case AR_OBJECT_RASTERIZER:
    case AR_OBJECT_DEPTHSTENCIL:
    case AR_OBJECT_BLEND:

    case AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC:
    case AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME:

    default:
      return QualType();
    }
  }

  /// <summary>Promotes the specified expression to an integer type if it's a boolean type.</summary
  /// <param name="E">Expression to typecast.</param>
  /// <returns>E typecast to a integer type if it's a valid boolean type; E otherwise.</returns>
  ExprResult PromoteToIntIfBool(ExprResult& E);

  QualType NewQualifiedType(UINT64 qwUsages, QualType type)
  {
    // NOTE: NewQualifiedType does quite a bit more in the prior compiler
    (void)(qwUsages);
    return type;
  }

  QualType NewSimpleAggregateType(
    _In_ ArTypeObjectKind ExplicitKind,
    _In_ ArBasicKind componentType,
    _In_ UINT64 qwQual,
    _In_ UINT uRows,
    _In_ UINT uCols)
  {
    DXASSERT_VALIDBASICKIND(componentType);

    QualType pType;  // The type to return.
    if (componentType < AR_BASIC_COUNT) {
      // If basic numeric, call LookupScalarTypeDef to ensure on-demand
      // initialization
      LookupScalarTypeDef(ScalarTypeForBasic(componentType));
    }
    QualType pEltType = GetBasicKindType(componentType);
    DXASSERT(!pEltType.isNull(), "otherwise caller is specifying an incorrect basic kind type");

    // TODO: handle adding qualifications like const
    pType = NewQualifiedType(
      qwQual & ~(UINT64)(AR_QUAL_COLMAJOR | AR_QUAL_ROWMAJOR),
      pEltType);

    if (uRows > 1 ||
      uCols > 1 ||
      ExplicitKind == AR_TOBJ_VECTOR ||
      ExplicitKind == AR_TOBJ_MATRIX)
    {
      HLSLScalarType scalarType = ScalarTypeForBasic(componentType);
      DXASSERT(scalarType != HLSLScalarType_unknown, "otherwise caller is specifying an incorrect type");

      if ((uRows == 1 &&
        ExplicitKind != AR_TOBJ_MATRIX) ||
        ExplicitKind == AR_TOBJ_VECTOR)
      {
        pType = LookupVectorType(scalarType, uCols);
      }
      else
      {
        pType = LookupMatrixType(scalarType, uRows, uCols);
      }

      // TODO: handle colmajor/rowmajor
      //if ((qwQual & (AR_QUAL_COLMAJOR | AR_QUAL_ROWMAJOR)) != 0)
      //{
      //  VN(pType = NewQualifiedType(pSrcLoc,
      //    qwQual & (AR_QUAL_COLMAJOR |
      //    AR_QUAL_ROWMAJOR),
      //    pMatrix));
      //}
      //else
      //{
      //  pType = pMatrix;
      //}
    }

    return pType;
  }

  /// <summary>Attempts to match Args to the signature specification in pIntrinsic.</summary>
  /// <param name="pIntrinsic">Intrinsic function to match.</param>
  /// <param name="objectElement">Type element on the class intrinsic belongs to; possibly null (eg, 'float' in 'Texture2D<float>').</param>
  /// <param name="Args">Invocation arguments to match.</param>
  /// <param name="argTypes">After exectuion, type of arguments.</param>
  /// <param name="argCount">After execution, number of arguments in argTypes.</param>
  /// <remarks>On success, argTypes includes the clang Types to use for the signature, with the first being the return type.</remarks>
  bool MatchArguments(
    const _In_ HLSL_INTRINSIC *pIntrinsic,
    _In_ QualType objectElement,
    _In_ QualType functionTemplateTypeArg,
    _In_ ArrayRef<Expr *> Args, 
    _Out_writes_(g_MaxIntrinsicParamCount + 1) QualType(&argTypes)[g_MaxIntrinsicParamCount + 1],
    _Out_range_(0, g_MaxIntrinsicParamCount + 1) size_t* argCount);

  /// <summary>Validate object element on intrinsic to catch case like integer on Sample.</summary>
  /// <param name="pIntrinsic">Intrinsic function to validate.</param>
  /// <param name="objectElement">Type element on the class intrinsic belongs to; possibly null (eg, 'float' in 'Texture2D<float>').</param>
  bool IsValidateObjectElement(
    _In_ const HLSL_INTRINSIC *pIntrinsic,
    _In_ QualType objectElement);

  // Returns the iterator with the first entry that matches the requirement
  IntrinsicDefIter FindIntrinsicByNameAndArgCount(
    _In_count_(tableSize) const HLSL_INTRINSIC* table,
    size_t tableSize,
    StringRef typeName,
    StringRef nameIdentifier,
    size_t argumentCount)
  {
    // This is implemented by a linear scan for now.
    // We tested binary search on tables, and there was no performance gain on
    // samples probably for the following reasons.
    // 1. The tables are not big enough to make noticable difference
    // 2. The user of this function assumes that it returns the first entry in
    // the table that matches name and argument count. So even in the binary
    // search, we have to scan backwards until the entry does not match the name
    // or arg count. For linear search this is not a problem
    for (unsigned int i = 0; i < tableSize; i++) {
      const HLSL_INTRINSIC* pIntrinsic = &table[i];

      // Do some quick checks to verify size and name.
      if (pIntrinsic->uNumArgs != 1 + argumentCount) {
        continue;
      }
      if (!nameIdentifier.equals(StringRef(pIntrinsic->pArgs[0].pName))) {
        continue;
      }

      return IntrinsicDefIter::CreateStart(table, tableSize, pIntrinsic,
        IntrinsicTableDefIter::CreateStart(m_intrinsicTables, typeName, nameIdentifier, argumentCount));
    }

    return IntrinsicDefIter::CreateStart(table, tableSize, table + tableSize,
      IntrinsicTableDefIter::CreateStart(m_intrinsicTables, typeName, nameIdentifier, argumentCount));
  }

  bool AddOverloadedCallCandidates(
    UnresolvedLookupExpr *ULE,
    ArrayRef<Expr *> Args,
    OverloadCandidateSet &CandidateSet,
    bool PartialOverloading) override
  {
    DXASSERT_NOMSG(ULE != nullptr);

    // Intrinsics live in the global namespace, so references to their names
    // should be either unqualified or '::'-prefixed.
    if (ULE->getQualifier() && ULE->getQualifier()->getKind() != NestedNameSpecifier::Global) {
      return false;
    }

    const DeclarationNameInfo declName = ULE->getNameInfo();
    IdentifierInfo* idInfo = declName.getName().getAsIdentifierInfo();
    if (idInfo == nullptr)
    {
      return false;
    }

    StringRef nameIdentifier = idInfo->getName();

    IntrinsicDefIter cursor = FindIntrinsicByNameAndArgCount(
      g_Intrinsics, _countof(g_Intrinsics), StringRef(), nameIdentifier, Args.size());
    IntrinsicDefIter end = IntrinsicDefIter::CreateEnd(
      g_Intrinsics, _countof(g_Intrinsics), IntrinsicTableDefIter::CreateEnd(m_intrinsicTables));
    while (cursor != end)
    {
      // If this is the intrinsic we're interested in, build up a representation
      // of the types we need.
      const HLSL_INTRINSIC* pIntrinsic = *cursor;
      LPCSTR tableName = cursor.GetTableName();
      LPCSTR lowering = cursor.GetLoweringStrategy();
      DXASSERT(
        pIntrinsic->uNumArgs <= g_MaxIntrinsicParamCount + 1,
        "otherwise g_MaxIntrinsicParamCount needs to be updated for wider signatures");
      QualType functionArgTypes[g_MaxIntrinsicParamCount + 1];
      size_t functionArgTypeCount = 0;
      if (!MatchArguments(pIntrinsic, QualType(), QualType(), Args, functionArgTypes, &functionArgTypeCount))
      {
        ++cursor;
        continue;
      }

      // Get or create the overload we're interested in.
      FunctionDecl* intrinsicFuncDecl = nullptr;
      std::pair<UsedIntrinsicStore::iterator, bool> insertResult = m_usedIntrinsics.insert(UsedIntrinsic(
        pIntrinsic, functionArgTypes, functionArgTypeCount));
      bool insertedNewValue = insertResult.second;
      if (insertedNewValue)
      {
        DXASSERT(tableName, "otherwise IDxcIntrinsicTable::GetTableName() failed");
        intrinsicFuncDecl = AddHLSLIntrinsicFunction(*m_context, m_hlslNSDecl, tableName, lowering, pIntrinsic, functionArgTypes, functionArgTypeCount);
        insertResult.first->setFunctionDecl(intrinsicFuncDecl);
      }
      else
      {
        intrinsicFuncDecl = (*insertResult.first).getFunctionDecl();
      }

      OverloadCandidate& candidate = CandidateSet.addCandidate();
      candidate.Function = intrinsicFuncDecl;
      candidate.FoundDecl.setDecl(intrinsicFuncDecl);
      candidate.Viable = true;

      return true;
    }

    return false;
  }

  bool Initialize(ASTContext& context)
  {
    m_context = &context;

    m_hlslNSDecl = NamespaceDecl::Create(context, context.getTranslationUnitDecl(),
                               /*Inline*/ false, SourceLocation(),
                               SourceLocation(), &context.Idents.get("hlsl"),
                               /*PrevDecl*/ nullptr);
    m_hlslNSDecl->setImplicit();
    AddBaseTypes();
    AddHLSLScalarTypes();
    AddHLSLStringType();

    AddHLSLVectorTemplate(*m_context, &m_vectorTemplateDecl);
    DXASSERT(m_vectorTemplateDecl != nullptr, "AddHLSLVectorTypes failed to return the vector template declaration");
    AddHLSLMatrixTemplate(*m_context, m_vectorTemplateDecl, &m_matrixTemplateDecl);
    DXASSERT(m_matrixTemplateDecl != nullptr, "AddHLSLMatrixTypes failed to return the matrix template declaration");

    // Initializing built in integers for ray tracing
    AddRayFlags(*m_context);
    AddHitKinds(*m_context);
    AddStateObjectFlags(*m_context);

    return true;
  }

  /// <summary>Checks whether the specified type is numeric or composed of numeric elements exclusively.</summary>
  bool IsTypeNumeric(QualType type, _Out_ UINT* count);

  /// <summary>Checks whether the specified type is a scalar type.</summary>
  bool IsScalarType(const QualType& type) {
    DXASSERT(!type.isNull(), "caller should validate its type is initialized");
    return BasicTypeForScalarType(type->getCanonicalTypeUnqualified()) != AR_BASIC_UNKNOWN;
  }

  /// <summary>Checks whether the specified value is a valid vector size.</summary>
  bool IsValidVectorSize(size_t length) {
    return 1 <= length && length <= 4;
  }

  /// <summary>Checks whether the specified value is a valid matrix row or column size.</summary>
  bool IsValidMatrixColOrRowSize(size_t length) {
    return 1 <= length && length <= 4;
  }

  bool IsValidTemplateArgumentType(SourceLocation argLoc, const QualType& type, bool requireScalar) {
    if (type.isNull()) {
      return false;
    }

    if (type.hasQualifiers()) {
      return false;
    }

    // TemplateTypeParm here will be construction of vector return template in matrix operator[]
    if (type->getTypeClass() == Type::TemplateTypeParm)
      return true;

    QualType qt = GetStructuralForm(type);

    if (requireScalar) {
      if (!IsScalarType(qt)) {
        m_sema->Diag(argLoc, diag::err_hlsl_typeintemplateargument_requires_scalar) << type;
        return false;
      }
      return true;
    }
    else {
      ArTypeObjectKind objectKind = GetTypeObjectKind(qt);

      if (qt->isArrayType()) {
        const ArrayType* arrayType = qt->getAsArrayTypeUnsafe();
        return IsValidTemplateArgumentType(argLoc, arrayType->getElementType(), false);
      }
      else if (objectKind == AR_TOBJ_VECTOR) {
        bool valid = true;
        if (!IsValidVectorSize(GetHLSLVecSize(type))) {
          valid = false;
          m_sema->Diag(argLoc, diag::err_hlsl_unsupportedvectorsize) << type << GetHLSLVecSize(type);
        }
        if (!IsScalarType(GetMatrixOrVectorElementType(type))) {
          valid = false;
          m_sema->Diag(argLoc, diag::err_hlsl_unsupportedvectortype) << type << GetMatrixOrVectorElementType(type);
        }
        return valid;
      }
      else if (objectKind == AR_TOBJ_MATRIX) {
        bool valid = true;
        UINT rowCount, colCount;
        GetRowsAndCols(type, rowCount, colCount);
        if (!IsValidMatrixColOrRowSize(rowCount) || !IsValidMatrixColOrRowSize(colCount)) {
          valid = false;
          m_sema->Diag(argLoc, diag::err_hlsl_unsupportedmatrixsize) << type << rowCount << colCount;
        }
        if (!IsScalarType(GetMatrixOrVectorElementType(type))) {
          valid = false;
          m_sema->Diag(argLoc, diag::err_hlsl_unsupportedvectortype) << type << GetMatrixOrVectorElementType(type);
        }
        return valid;
      }
      else if (qt->isStructureType()) {
        const RecordType* recordType = qt->getAsStructureType();
        objectKind = ClassifyRecordType(recordType);
        switch (objectKind)
        {
        case AR_TOBJ_OBJECT:
          m_sema->Diag(argLoc, diag::err_hlsl_objectintemplateargument) << type;
          return false;
        case AR_TOBJ_COMPOUND:
          {
            const RecordDecl* recordDecl = recordType->getDecl();
            RecordDecl::field_iterator begin = recordDecl->field_begin();
            RecordDecl::field_iterator end = recordDecl->field_end();
            bool result = true;
            while (begin != end) {
              const FieldDecl* fieldDecl = *begin;
              if (!IsValidTemplateArgumentType(argLoc, fieldDecl->getType(), false)) {
                m_sema->Diag(argLoc, diag::note_field_type_usage)
                  << fieldDecl->getType() << fieldDecl->getIdentifier() << type;
                result = false;
              }
              begin++;
            }
            return result;
          }
        default:
          m_sema->Diag(argLoc, diag::err_hlsl_typeintemplateargument) << type;
          return false;
        }
      }
      else if(IsScalarType(qt)) {
        return true;
      }
      else {
        m_sema->Diag(argLoc, diag::err_hlsl_typeintemplateargument) << type;
        return false;
      }
    }
  }

  /// <summary>Checks whether the source type can be converted to the target type.</summary>
  bool CanConvert(SourceLocation loc, Expr* sourceExpr, QualType target, bool explicitConversion,
    _Out_opt_ TYPE_CONVERSION_REMARKS* remarks,
    _Inout_opt_ StandardConversionSequence* sequence);
  void CollectInfo(QualType type, _Out_ ArTypeInfo* pTypeInfo);
  void GetConversionForm(
    QualType type,
    bool explicitConversion,
    ArTypeInfo* pTypeInfo);
  bool ValidateCast(SourceLocation Loc, _In_ Expr* source, QualType target, bool explicitConversion,
     bool suppressWarnings, bool suppressErrors,
    _Inout_opt_ StandardConversionSequence* sequence);
  bool ValidatePrimitiveTypeForOperand(SourceLocation loc, QualType type, ArTypeObjectKind kind);
  bool ValidateTypeRequirements(
    SourceLocation loc,
    ArBasicKind elementKind,
    ArTypeObjectKind objectKind,
    bool requiresIntegrals,
    bool requiresNumerics);

  /// <summary>Validates and adjusts operands for the specified binary operator.</summary>
  /// <param name="OpLoc">Source location for operator.</param>
  /// <param name="Opc">Kind of binary operator.</param>
  /// <param name="LHS">Left-hand-side expression, possibly updated by this function.</param>
  /// <param name="RHS">Right-hand-side expression, possibly updated by this function.</param>
  /// <param name="ResultTy">Result type for operator expression.</param>
  /// <param name="CompLHSTy">Type of LHS after promotions for computation.</param>
  /// <param name="CompResultTy">Type of computation result.</param>
  void CheckBinOpForHLSL(
    SourceLocation OpLoc,
    BinaryOperatorKind Opc,
    ExprResult& LHS,
    ExprResult& RHS,
    QualType& ResultTy,
    QualType& CompLHSTy,
    QualType& CompResultTy);

  /// <summary>Validates and adjusts operands for the specified unary operator.</summary>
  /// <param name="OpLoc">Source location for operator.</param>
  /// <param name="Opc">Kind of operator.</param>
  /// <param name="InputExpr">Input expression to the operator.</param>
  /// <param name="VK">Value kind for resulting expression.</param>
  /// <param name="OK">Object kind for resulting expression.</param>
  /// <returns>The result type for the expression.</returns>
  QualType CheckUnaryOpForHLSL(
    SourceLocation OpLoc,
    UnaryOperatorKind Opc,
    ExprResult& InputExpr,
    ExprValueKind& VK,
    ExprObjectKind& OK);

  /// <summary>Checks vector conditional operator (Cond ? LHS : RHS).</summary>
  /// <param name="Cond">Vector condition expression.</param>
  /// <param name="LHS">Left hand side.</param>
  /// <param name="RHS">Right hand side.</param>
  /// <param name="QuestionLoc">Location of question mark in operator.</param>
  /// <returns>Result type of vector conditional expression.</returns>
  clang::QualType CheckVectorConditional(
    _In_ ExprResult &Cond,
    _In_ ExprResult &LHS,
    _In_ ExprResult &RHS,
    _In_ SourceLocation QuestionLoc);

  clang::QualType ApplyTypeSpecSignToParsedType(
      _In_ clang::QualType &type,
      _In_ TypeSpecifierSign TSS,
      _In_ SourceLocation Loc
  );

  bool CheckRangedTemplateArgument(SourceLocation diagLoc, llvm::APSInt& sintValue)
  {
    if (!sintValue.isStrictlyPositive() || sintValue.getLimitedValue() > 4)
    {
      m_sema->Diag(diagLoc, diag::err_hlsl_invalid_range_1_4);
      return true;
    }

    return false;
  }

  /// <summary>Performs HLSL-specific processing of template declarations.</summary>
  bool
  CheckTemplateArgumentListForHLSL(_In_ TemplateDecl *Template,
                                   SourceLocation /* TemplateLoc */,
                                   TemplateArgumentListInfo &TemplateArgList) {
    DXASSERT_NOMSG(Template != nullptr);

    // Determine which object type the template refers to.
    StringRef templateName = Template->getName();

    // NOTE: this 'escape valve' allows unit tests to perform type checks.
    if (templateName.equals(StringRef("is_same"))) {
      return false;
    }

    bool isMatrix = Template->getCanonicalDecl() ==
                    m_matrixTemplateDecl->getCanonicalDecl();
    bool isVector = Template->getCanonicalDecl() ==
                    m_vectorTemplateDecl->getCanonicalDecl();
    bool requireScalar = isMatrix || isVector;
    
    // Check constraints on the type. Right now we only check that template
    // types are primitive types.
    for (unsigned int i = 0; i < TemplateArgList.size(); i++) {
      const TemplateArgumentLoc &argLoc = TemplateArgList[i];
      SourceLocation argSrcLoc = argLoc.getLocation();
      const TemplateArgument &arg = argLoc.getArgument();
      if (arg.getKind() == TemplateArgument::ArgKind::Type) {
        QualType argType = arg.getAsType();
        if (!IsValidTemplateArgumentType(argSrcLoc, argType, requireScalar)) {
          // NOTE: IsValidTemplateArgumentType emits its own diagnostics
          return true;
        }
      }
      else if (arg.getKind() == TemplateArgument::ArgKind::Expression) {
        if (isMatrix || isVector) {
          Expr *expr = arg.getAsExpr();
          llvm::APSInt constantResult;
          if (expr != nullptr &&
              expr->isIntegerConstantExpr(constantResult, *m_context)) {
            if (CheckRangedTemplateArgument(argSrcLoc, constantResult)) {
              return true;
            }
          }
        }
      }
      else if (arg.getKind() == TemplateArgument::ArgKind::Integral) {
        if (isMatrix || isVector) {
          llvm::APSInt Val = arg.getAsIntegral();
          if (CheckRangedTemplateArgument(argSrcLoc, Val)) {
            return true;
          }
        }
      }
    }

    return false;
  }

  FindStructBasicTypeResult FindStructBasicType(_In_ DeclContext* functionDeclContext);

  /// <summary>Finds the table of intrinsics for the declaration context of a member function.</summary>
  /// <param name="functionDeclContext">Declaration context of function.</param>
  /// <param name="name">After execution, the name of the object to which the table applies.</param>
  /// <param name="intrinsics">After execution, the intrinsic table.</param>
  /// <param name="intrinsicCount">After execution, the count of elements in the intrinsic table.</param>
  void FindIntrinsicTable(
    _In_ DeclContext* functionDeclContext,
    _Outptr_result_z_ const char** name,
    _Outptr_result_buffer_(*intrinsicCount) const HLSL_INTRINSIC** intrinsics,
    _Out_ size_t* intrinsicCount);

  /// <summary>Deduces the template arguments by comparing the argument types and the HLSL intrinsic tables.</summary>
  /// <param name="FunctionTemplate">The declaration for the function template being deduced.</param>
  /// <param name="ExplicitTemplateArgs">Explicitly-provided template arguments. Should be empty for an HLSL program.</param>
  /// <param name="Args">Array of expressions being used as arguments.</param>
  /// <param name="Specialization">The declaration for the resolved specialization.</param>
  /// <param name="Info">Provides information about an attempted template argument deduction.</param>
  /// <returns>The result of the template deduction, TDK_Invalid if no HLSL-specific processing done.</returns>
  Sema::TemplateDeductionResult DeduceTemplateArgumentsForHLSL(
    FunctionTemplateDecl *FunctionTemplate,
    TemplateArgumentListInfo *ExplicitTemplateArgs, ArrayRef<Expr *> Args,
    FunctionDecl *&Specialization, TemplateDeductionInfo &Info);

  clang::OverloadingResult GetBestViableFunction(
    clang::SourceLocation Loc,
    clang::OverloadCandidateSet& set,
    clang::OverloadCandidateSet::iterator& Best);

  /// <summary>
  /// Initializes the specified <paramref name="initSequence" /> describing how
  /// <paramref name="Entity" /> is initialized with <paramref name="Args" />.
  /// </summary>
  /// <param name="Entity">Entity being initialized; a variable, return result, etc.</param>
  /// <param name="Kind">Kind of initialization: copying, list-initializing, constructing, etc.</param>
  /// <param name="Args">Arguments to the initialization.</param>
  /// <param name="TopLevelOfInitList">Whether this is the top-level of an initialization list.</param>
  /// <param name="initSequence">Initialization sequence description to initialize.</param>
  void InitializeInitSequenceForHLSL(
    const InitializedEntity& Entity,
    const InitializationKind& Kind,
    MultiExprArg Args,
    bool TopLevelOfInitList,
    _Inout_ InitializationSequence* initSequence);

  /// <summary>
  /// Checks whether the specified conversion occurs to a type of idential element type but less elements.
  /// </summary>
  /// <remarks>This is an important case because a cast of this type does not turn an lvalue into an rvalue.</remarks>
  bool IsConversionToLessOrEqualElements(
    const ExprResult& sourceExpr,
    const QualType& targetType,
    bool explicitConversion);

  /// <summary>
  /// Checks whether the specified conversion occurs to a type of idential element type but less elements.
  /// </summary>
  /// <remarks>This is an important case because a cast of this type does not turn an lvalue into an rvalue.</remarks>
  bool IsConversionToLessOrEqualElements(
    const QualType& sourceType,
    const QualType& targetType,
    bool explicitConversion);

  /// <summary>Performs a member lookup on the specified BaseExpr if it's a matrix.</summary>
  /// <param name="BaseExpr">Base expression for member access.</param>
  /// <param name="MemberName">Name of member to look up.</param>
  /// <param name="IsArrow">Whether access is through arrow (a->b) rather than period (a.b).</param>
  /// <param name="OpLoc">Location of access operand.</param>
  /// <param name="MemberLoc">Location of member.</param>
  /// <param name="result">Result of lookup operation.</param>
  /// <returns>true if the base type is a matrix and the lookup has been handled.</returns>
  bool LookupMatrixMemberExprForHLSL(
    Expr& BaseExpr,
    DeclarationName MemberName,
    bool IsArrow,
    SourceLocation OpLoc,
    SourceLocation MemberLoc,
    ExprResult* result);

  /// <summary>Performs a member lookup on the specified BaseExpr if it's a vector.</summary>
  /// <param name="BaseExpr">Base expression for member access.</param>
  /// <param name="MemberName">Name of member to look up.</param>
  /// <param name="IsArrow">Whether access is through arrow (a->b) rather than period (a.b).</param>
  /// <param name="OpLoc">Location of access operand.</param>
  /// <param name="MemberLoc">Location of member.</param>
  /// <param name="result">Result of lookup operation.</param>
  /// <returns>true if the base type is a vector and the lookup has been handled.</returns>
  bool LookupVectorMemberExprForHLSL(
    Expr& BaseExpr,
    DeclarationName MemberName,
    bool IsArrow,
    SourceLocation OpLoc,
    SourceLocation MemberLoc,
    ExprResult* result);

  /// <summary>Performs a member lookup on the specified BaseExpr if it's an array.</summary>
  /// <param name="BaseExpr">Base expression for member access.</param>
  /// <param name="MemberName">Name of member to look up.</param>
  /// <param name="IsArrow">Whether access is through arrow (a->b) rather than period (a.b).</param>
  /// <param name="OpLoc">Location of access operand.</param>
  /// <param name="MemberLoc">Location of member.</param>
  /// <param name="result">Result of lookup operation.</param>
  /// <returns>true if the base type is an array and the lookup has been handled.</returns>
  bool LookupArrayMemberExprForHLSL(
    Expr& BaseExpr,
    DeclarationName MemberName,
    bool IsArrow,
    SourceLocation OpLoc,
    SourceLocation MemberLoc,
    ExprResult* result);

  /// <summary>If E is a scalar, converts it to a 1-element vector.</summary>
  /// <param name="E">Expression to convert.</param>
  /// <returns>The result of the conversion; or E if the type is not a scalar.</returns>
  ExprResult MaybeConvertScalarToVector(_In_ clang::Expr* E);

  clang::Expr *HLSLImpCastToScalar(
    _In_ clang::Sema* self,
    _In_ clang::Expr* From,
    ArTypeObjectKind FromShape,
    ArBasicKind EltKind);
  clang::ExprResult PerformHLSLConversion(
    _In_ clang::Expr* From,
    _In_ clang::QualType targetType,
    _In_ const clang::StandardConversionSequence &SCS,
    _In_ clang::Sema::CheckedConversionKind CCK);

  /// <summary>Diagnoses an error when precessing the specified type if nesting is too deep.</summary>
  void ReportUnsupportedTypeNesting(SourceLocation loc, QualType type);

  /// <summary>
  /// Checks if a static cast can be performed, and performs it if possible.
  /// </summary>
  /// <param name="SrcExpr">Expression to cast.</param>
  /// <param name="DestType">Type to cast SrcExpr to.</param>
  /// <param name="CCK">Kind of conversion: implicit, C-style, functional, other.</param>
  /// <param name="OpRange">Source range for the cast operation.</param>
  /// <param name="msg">Error message from the diag::* enumeration to fail with; zero to suppress messages.</param>
  /// <param name="Kind">The kind of operation required for a conversion.</param>
  /// <param name="BasePath">A simple array of base specifiers.</param>
  /// <param name="ListInitialization">Whether the cast is in the context of a list initialization.</param>
  /// <param name="SuppressWarnings">Whether warnings should be omitted.</param>
  /// <param name="SuppressErrors">Whether errors should be omitted.</param>
  bool TryStaticCastForHLSL(ExprResult &SrcExpr,
    QualType DestType,
    Sema::CheckedConversionKind CCK,
    const SourceRange &OpRange, unsigned &msg,
    CastKind &Kind, CXXCastPath &BasePath,
    bool ListInitialization, bool SuppressWarnings, bool SuppressErrors,
    _Inout_opt_ StandardConversionSequence* standard);

  /// <summary>
  /// Checks if a subscript index argument can be initialized from the given expression.
  /// </summary>
  /// <param name="SrcExpr">Source expression used as argument.</param>
  /// <param name="DestType">Parameter type to initialize.</param>
  /// <remarks>
  /// Rules for subscript index initialization follow regular implicit casting rules, with the exception that
  /// no changes in arity are allowed (i.e., int2 can become uint2, but uint or uint3 cannot).
  /// </remarks>
  ImplicitConversionSequence TrySubscriptIndexInitialization(_In_ clang::Expr* SrcExpr, clang::QualType DestType);

  void AddHLSLObjectMethodsIfNotReady(QualType qt) {
    static_assert((sizeof(uint64_t)*8) >= _countof(g_ArBasicKindsAsTypes), "Bitmask size is too small");
    // Everything is ready.
    if (m_objectTypeLazyInitMask == 0)
      return;
    CXXRecordDecl *recordDecl = const_cast<CXXRecordDecl *>(GetRecordDeclForBuiltInOrStruct(qt->getAsCXXRecordDecl()));
    int idx = FindObjectBasicKindIndex(recordDecl);
    // Not object type.
    if (idx == -1)
      return;
    uint64_t bit = ((uint64_t)1)<<idx;
    // Already created.
    if ((m_objectTypeLazyInitMask & bit) == 0)
      return;
    
    ArBasicKind kind = g_ArBasicKindsAsTypes[idx];
    uint8_t templateArgCount = g_ArBasicKindsTemplateCount[idx];
    
    int startDepth = 0;

    if (templateArgCount > 0) {
      DXASSERT(templateArgCount == 1 || templateArgCount == 2,
               "otherwise a new case has been added");
      ClassTemplateDecl *typeDecl = recordDecl->getDescribedClassTemplate();
      AddObjectSubscripts(kind, typeDecl, recordDecl,
                          g_ArBasicKindsSubscripts[idx]);
      startDepth = 1;
    }

    AddObjectMethods(kind, recordDecl, startDepth);
    // Clear the object.
    m_objectTypeLazyInitMask &= ~bit;
  }

  FunctionDecl* AddHLSLIntrinsicMethod(
    LPCSTR tableName,
    LPCSTR lowering,
    _In_ const HLSL_INTRINSIC* intrinsic,
    _In_ FunctionTemplateDecl *FunctionTemplate,
    ArrayRef<Expr *> Args,
    _In_count_(parameterTypeCount) QualType* parameterTypes,
    size_t parameterTypeCount)
  {
    DXASSERT_NOMSG(intrinsic != nullptr);
    DXASSERT_NOMSG(FunctionTemplate != nullptr);
    DXASSERT_NOMSG(parameterTypes != nullptr);
    DXASSERT(parameterTypeCount >= 1, "otherwise caller didn't initialize - there should be at least a void return type");

    // Create the template arguments.
    SmallVector<TemplateArgument, g_MaxIntrinsicParamCount + 1> templateArgs;
    for (size_t i = 0; i < parameterTypeCount; i++) {
      templateArgs.push_back(TemplateArgument(parameterTypes[i]));
    }

    // Look for an existing specialization.
    void *InsertPos = nullptr;
    FunctionDecl *SpecFunc =
        FunctionTemplate->findSpecialization(templateArgs, InsertPos);
    if (SpecFunc != nullptr) {
      return SpecFunc;
    }

    // Change return type to lvalue reference type for aggregate types
    QualType retTy = parameterTypes[0];
    if (hlsl::IsHLSLAggregateType(retTy))
      parameterTypes[0] = m_context->getLValueReferenceType(retTy);

    // Create a new specialization.
    SmallVector<ParameterModifier, g_MaxIntrinsicParamCount> paramMods;
    InitParamMods(intrinsic, paramMods);

    for (unsigned int i = 1; i < parameterTypeCount; i++) {
      // Change out/inout parameter type to rvalue reference type.
      if (paramMods[i - 1].isAnyOut()) {
        parameterTypes[i] = m_context->getLValueReferenceType(parameterTypes[i]);
      }
    }

    IntrinsicOp intrinOp = static_cast<IntrinsicOp>(intrinsic->Op);

    if (intrinOp == IntrinsicOp::MOP_SampleBias) {
      // Remove this when update intrinsic table not affect other things.
      // Change vector<float,1> into float for bias.
      const unsigned biasOperandID = 3; // return type, sampler, coord, bias.
      DXASSERT(parameterTypeCount > biasOperandID,
               "else operation was misrecognized");
      if (const ExtVectorType *VecTy =
              hlsl::ConvertHLSLVecMatTypeToExtVectorType(
                  *m_context, parameterTypes[biasOperandID])) {
        if (VecTy->getNumElements() == 1)
          parameterTypes[biasOperandID] = VecTy->getElementType();
      }
    }

    DeclContext *owner = FunctionTemplate->getDeclContext();
    TemplateArgumentList templateArgumentList(
        TemplateArgumentList::OnStackType::OnStack, templateArgs.data(),
        templateArgs.size());
    MultiLevelTemplateArgumentList mlTemplateArgumentList(templateArgumentList);
    TemplateDeclInstantiator declInstantiator(*this->m_sema, owner,
                                              mlTemplateArgumentList);
    FunctionProtoType::ExtProtoInfo EmptyEPI;
    QualType functionType = m_context->getFunctionType(
        parameterTypes[0],
        ArrayRef<QualType>(parameterTypes + 1, parameterTypeCount - 1),
        EmptyEPI, paramMods);
    TypeSourceInfo *TInfo = m_context->CreateTypeSourceInfo(functionType, 0);
    FunctionProtoTypeLoc Proto =
        TInfo->getTypeLoc().getAs<FunctionProtoTypeLoc>();

    SmallVector<ParmVarDecl*, g_MaxIntrinsicParamCount> Params;
    for (unsigned int i = 1; i < parameterTypeCount; i++) {
      IdentifierInfo* id = &m_context->Idents.get(StringRef(intrinsic->pArgs[i - 1].pName));
      ParmVarDecl *paramDecl = ParmVarDecl::Create(
          *m_context, nullptr, NoLoc, NoLoc, id, parameterTypes[i], nullptr,
          StorageClass::SC_None, nullptr, paramMods[i - 1]);
      Params.push_back(paramDecl);
    }

    QualType T = TInfo->getType();
    DeclarationNameInfo NameInfo(FunctionTemplate->getDeclName(), NoLoc);
    CXXMethodDecl* method = CXXMethodDecl::Create(
      *m_context, dyn_cast<CXXRecordDecl>(owner), NoLoc, NameInfo, T, TInfo,
      SC_Extern, InlineSpecifiedFalse, IsConstexprFalse, NoLoc);

    // Add intrinsic attr
    AddHLSLIntrinsicAttr(method, *m_context, tableName, lowering, intrinsic);

    // Record this function template specialization.
    TemplateArgumentList *argListCopy = TemplateArgumentList::CreateCopy(
        *m_context, templateArgs.data(), templateArgs.size());
    method->setFunctionTemplateSpecialization(FunctionTemplate, argListCopy, 0);

    // Attach the parameters
    for (unsigned P = 0; P < Params.size(); ++P) {
      Params[P]->setOwningFunction(method);
      Proto.setParam(P, Params[P]);
    }
    method->setParams(Params);

    // Adjust access.
    method->setAccess(AccessSpecifier::AS_public);
    FunctionTemplate->setAccess(method->getAccess());

    return method;
  }

  // Overload support.
  UINT64 ScoreCast(QualType leftType, QualType rightType);
  UINT64 ScoreFunction(OverloadCandidateSet::iterator &Cand);
  UINT64 ScoreImplicitConversionSequence(const ImplicitConversionSequence *s);
  unsigned GetNumElements(QualType anyType);
  unsigned GetNumBasicElements(QualType anyType);
  unsigned GetNumConvertCheckElts(QualType leftType, unsigned leftSize, QualType rightType, unsigned rightSize);
  QualType GetNthElementType(QualType type, unsigned index);
  bool IsPromotion(ArBasicKind leftKind, ArBasicKind rightKind);
  bool IsCast(ArBasicKind leftKind, ArBasicKind rightKind);
  bool IsIntCast(ArBasicKind leftKind, ArBasicKind rightKind);
};

TYPE_CONVERSION_REMARKS HLSLExternalSource::RemarksUnused = TYPE_CONVERSION_REMARKS::TYPE_CONVERSION_NONE;
ImplicitConversionKind HLSLExternalSource::ImplicitConversionKindUnused = ImplicitConversionKind::ICK_Identity;

// Use this class to flatten a type into HLSL primitives and iterate through them.
class FlattenedTypeIterator
{
private:
  enum FlattenedIterKind {
    FK_Simple,
    FK_Fields,
    FK_Expressions,
    FK_IncompleteArray,
    FK_Bases,
  };

  // Use this struct to represent a specific point in the tracked tree.
  struct FlattenedTypeTracker {
    QualType Type;                            // Type at this position in the tree.
    unsigned int Count;                       // Count of consecutive types
    CXXRecordDecl::base_class_iterator CurrentBase; // Current base for a structure type.
    CXXRecordDecl::base_class_iterator EndBase;     // STL-style end of bases.
    RecordDecl::field_iterator CurrentField;  // Current field in for a structure type.
    RecordDecl::field_iterator EndField;      // STL-style end of fields.
    MultiExprArg::iterator CurrentExpr;       // Current expression (advanceable for a list of expressions).
    MultiExprArg::iterator EndExpr;           // STL-style end of expressions.
    FlattenedIterKind IterKind;               // Kind of tracker.
    bool   IsConsidered;                      // If a FlattenedTypeTracker already been considered.

    FlattenedTypeTracker(QualType type)
        : Type(type), Count(0), CurrentExpr(nullptr),
          IterKind(FK_IncompleteArray), IsConsidered(false) {}
    FlattenedTypeTracker(QualType type, unsigned int count,
                         MultiExprArg::iterator expression)
        : Type(type), Count(count), CurrentExpr(expression),
          IterKind(FK_Simple), IsConsidered(false) {}
    FlattenedTypeTracker(QualType type, RecordDecl::field_iterator current,
                         RecordDecl::field_iterator end)
        : Type(type), Count(0), CurrentField(current), EndField(end),
          CurrentExpr(nullptr), IterKind(FK_Fields), IsConsidered(false) {}
    FlattenedTypeTracker(MultiExprArg::iterator current,
                         MultiExprArg::iterator end)
        : Count(0), CurrentExpr(current), EndExpr(end),
          IterKind(FK_Expressions), IsConsidered(false) {}
    FlattenedTypeTracker(QualType type,
                         CXXRecordDecl::base_class_iterator current,
                         CXXRecordDecl::base_class_iterator end)
        : Count(0), CurrentBase(current), EndBase(end), CurrentExpr(nullptr),
          IterKind(FK_Bases), IsConsidered(false) {}

    /// <summary>Gets the current expression if one is available.</summary>
    Expr* getExprOrNull() const { return CurrentExpr ? *CurrentExpr : nullptr; }
    /// <summary>Replaces the current expression.</summary>
    void replaceExpr(Expr* e) { *CurrentExpr = e; }
  };

  HLSLExternalSource& m_source;                         // Source driving the iteration.
  SmallVector<FlattenedTypeTracker, 4> m_typeTrackers;  // Active stack of trackers.
  bool m_draining;                                      // Whether the iterator is meant to drain (will not generate new elements in incomplete arrays).
  bool m_springLoaded;                                  // Whether the current element has been set up by an incomplete array but hasn't been used yet.
  unsigned int m_incompleteCount;                       // The number of elements in an incomplete array.
  size_t m_typeDepth;                                   // Depth of type analysis, to avoid stack overflows.
  QualType m_firstType;                                 // Name of first type found, used for diagnostics.
  SourceLocation m_loc;                                 // Location used for diagnostics.
  static const size_t MaxTypeDepth = 100;

  void advanceLeafTracker();
  /// <summary>Consumes leaves.</summary>
  void consumeLeaf();
  /// <summary>Considers whether the leaf has a usable expression without consuming anything.</summary>
  bool considerLeaf();
  /// <summary>Pushes a tracker for the specified expression; returns true if there is something to evaluate.</summary>
  bool pushTrackerForExpression(MultiExprArg::iterator expression);
  /// <summary>Pushes a tracker for the specified type; returns true if there is something to evaluate.</summary>
  bool pushTrackerForType(QualType type, _In_opt_ MultiExprArg::iterator expression);

public:
  /// <summary>Constructs a FlattenedTypeIterator for the specified type.</summary>
  FlattenedTypeIterator(SourceLocation loc, QualType type, HLSLExternalSource& source);
  /// <summary>Constructs a FlattenedTypeIterator for the specified arguments.</summary>
  FlattenedTypeIterator(SourceLocation loc, MultiExprArg args, HLSLExternalSource& source);

  /// <summary>Gets the current element in the flattened type hierarchy.</summary>
  QualType getCurrentElement() const;
  /// <summary>Get the number of repeated current elements.</summary>
  unsigned int getCurrentElementSize() const;
  /// <summary>Checks whether the iterator has a current element type to report.</summary>
  bool hasCurrentElement() const;
  /// <summary>Consumes count elements on this iterator.</summary>
  void advanceCurrentElement(unsigned int count);
  /// <summary>Counts the remaining elements in this iterator (consuming all elements).</summary>
  unsigned int countRemaining();
  /// <summary>Gets the current expression if one is available.</summary>
  Expr* getExprOrNull() const { return m_typeTrackers.back().getExprOrNull(); }
  /// <summary>Replaces the current expression.</summary>
  void replaceExpr(Expr* e) { m_typeTrackers.back().replaceExpr(e); }

  struct ComparisonResult
  {
    unsigned int LeftCount;
    unsigned int RightCount;

    /// <summary>Whether elements from right sequence are identical into left sequence elements.</summary>
    bool AreElementsEqual;

    /// <summary>Whether elements from right sequence can be converted into left sequence elements.</summary>
    bool CanConvertElements;

    /// <summary>Whether the elements can be converted and the sequences have the same length.</summary>
    bool IsConvertibleAndEqualLength() const {
      return CanConvertElements && LeftCount == RightCount;
    }

    /// <summary>Whether the elements can be converted but the left-hand sequence is longer.</summary>
    bool IsConvertibleAndLeftLonger() const {
      return CanConvertElements && LeftCount > RightCount;
    }

    bool IsRightLonger() const {
      return RightCount > LeftCount;
    }

    bool IsEqualLength() const {
      return LeftCount == RightCount;
    }
  };

  static ComparisonResult CompareIterators(
    HLSLExternalSource& source, SourceLocation loc,
    FlattenedTypeIterator& leftIter, FlattenedTypeIterator& rightIter);
  static ComparisonResult CompareTypes(
    HLSLExternalSource& source,
    SourceLocation leftLoc, SourceLocation rightLoc,
    QualType left, QualType right);
  // Compares the arguments to initialize the left type, modifying them if necessary.
  static ComparisonResult CompareTypesForInit(
    HLSLExternalSource& source, QualType left, MultiExprArg args,
    SourceLocation leftLoc, SourceLocation rightLoc);
};


static
QualType GetFirstElementTypeFromDecl(const Decl* decl)
{
  const ClassTemplateSpecializationDecl* specialization = dyn_cast<ClassTemplateSpecializationDecl>(decl);
  if (specialization) {
    const TemplateArgumentList& list = specialization->getTemplateArgs();
    if (list.size()) {
      return list[0].getAsType();
    }
  }

  return QualType();
}

void HLSLExternalSource::AddBaseTypes()
{
  DXASSERT(m_baseTypes[HLSLScalarType_unknown].isNull(), "otherwise unknown was initialized to an actual type");
  m_baseTypes[HLSLScalarType_bool] = m_context->BoolTy;
  m_baseTypes[HLSLScalarType_int] = m_context->IntTy;
  m_baseTypes[HLSLScalarType_uint] = m_context->UnsignedIntTy;
  m_baseTypes[HLSLScalarType_dword] = m_context->UnsignedIntTy;
  m_baseTypes[HLSLScalarType_half] = m_context->getLangOpts().UseMinPrecision ? m_context->HalfFloatTy : m_context->HalfTy;
  m_baseTypes[HLSLScalarType_float] = m_context->FloatTy;
  m_baseTypes[HLSLScalarType_double] = m_context->DoubleTy;
  m_baseTypes[HLSLScalarType_float_min10] = m_context->Min10FloatTy;
  m_baseTypes[HLSLScalarType_float_min16] = m_context->Min16FloatTy;
  m_baseTypes[HLSLScalarType_int_min12] = m_context->Min12IntTy;
  m_baseTypes[HLSLScalarType_int_min16] = m_context->Min16IntTy;
  m_baseTypes[HLSLScalarType_uint_min16] = m_context->Min16UIntTy;
  m_baseTypes[HLSLScalarType_float_lit] = m_context->LitFloatTy;
  m_baseTypes[HLSLScalarType_int_lit] = m_context->LitIntTy;
  m_baseTypes[HLSLScalarType_int16] = m_context->ShortTy;
  m_baseTypes[HLSLScalarType_int32] = m_context->IntTy;
  m_baseTypes[HLSLScalarType_int64] = m_context->LongLongTy;
  m_baseTypes[HLSLScalarType_uint16] = m_context->UnsignedShortTy;
  m_baseTypes[HLSLScalarType_uint32] = m_context->UnsignedIntTy;
  m_baseTypes[HLSLScalarType_uint64] = m_context->UnsignedLongLongTy;
  m_baseTypes[HLSLScalarType_float16] = m_context->HalfTy;
  m_baseTypes[HLSLScalarType_float32] = m_context->FloatTy;
  m_baseTypes[HLSLScalarType_float64] = m_context->DoubleTy;
}

void HLSLExternalSource::AddHLSLScalarTypes()
{
  DXASSERT(m_scalarTypes[HLSLScalarType_unknown].isNull(), "otherwise unknown was initialized to an actual type");
  m_scalarTypes[HLSLScalarType_bool] = m_baseTypes[HLSLScalarType_bool];
  m_scalarTypes[HLSLScalarType_int] = m_baseTypes[HLSLScalarType_int];
  m_scalarTypes[HLSLScalarType_float] = m_baseTypes[HLSLScalarType_float];
  m_scalarTypes[HLSLScalarType_double] = m_baseTypes[HLSLScalarType_double];
  m_scalarTypes[HLSLScalarType_float_lit] = m_baseTypes[HLSLScalarType_float_lit];
  m_scalarTypes[HLSLScalarType_int_lit] = m_baseTypes[HLSLScalarType_int_lit];
}

void HLSLExternalSource::AddHLSLStringType() {
  m_hlslStringType = m_context->HLSLStringTy;
}

FunctionDecl* HLSLExternalSource::AddSubscriptSpecialization(
  _In_ FunctionTemplateDecl* functionTemplate,
  QualType objectElement,
  const FindStructBasicTypeResult& findResult)
{
  DXASSERT_NOMSG(functionTemplate != nullptr);
  DXASSERT_NOMSG(!objectElement.isNull());
  DXASSERT_NOMSG(findResult.Found());
  DXASSERT(
    g_ArBasicKindsSubscripts[findResult.BasicKindsAsTypeIndex].SubscriptCardinality > 0,
    "otherwise the template shouldn't have an operator[] that the caller is trying to specialize");

  // Subscript is templated only on its return type.

  // Create the template argument.
  bool isReadWrite = GetBasicKindProps(findResult.Kind) & BPROP_RWBUFFER;
  QualType resultType = objectElement;
  if (!isReadWrite) resultType = m_context->getConstType(resultType);
  resultType = m_context->getLValueReferenceType(resultType);

  TemplateArgument templateArgument(resultType);
  unsigned subscriptCardinality =
      g_ArBasicKindsSubscripts[findResult.BasicKindsAsTypeIndex].SubscriptCardinality;
  QualType subscriptIndexType =
      subscriptCardinality == 1
          ? m_context->UnsignedIntTy
          : NewSimpleAggregateType(AR_TOBJ_VECTOR, AR_BASIC_UINT32, 0, 1,
                                   subscriptCardinality);

  // Look for an existing specialization.
  void* InsertPos = nullptr;
  FunctionDecl *SpecFunc = functionTemplate->findSpecialization(ArrayRef<TemplateArgument>(&templateArgument, 1), InsertPos);
  if (SpecFunc != nullptr) {
    return SpecFunc;
  }

  // Create a new specialization.
  DeclContext* owner = functionTemplate->getDeclContext();
  TemplateArgumentList templateArgumentList(
    TemplateArgumentList::OnStackType::OnStack, &templateArgument, 1);
  MultiLevelTemplateArgumentList mlTemplateArgumentList(templateArgumentList);
  TemplateDeclInstantiator declInstantiator(*this->m_sema, owner, mlTemplateArgumentList);
  const FunctionType *templateFnType = functionTemplate->getTemplatedDecl()->getType()->getAs<FunctionType>();
  const FunctionProtoType *protoType = dyn_cast<FunctionProtoType>(templateFnType);
  FunctionProtoType::ExtProtoInfo templateEPI = protoType->getExtProtoInfo();
  QualType functionType = m_context->getFunctionType(
    resultType, subscriptIndexType, templateEPI, None);
  TypeSourceInfo *TInfo = m_context->CreateTypeSourceInfo(functionType, 0);
  FunctionProtoTypeLoc Proto = TInfo->getTypeLoc().getAs<FunctionProtoTypeLoc>();

  IdentifierInfo* id = &m_context->Idents.get(StringRef("index"));
  ParmVarDecl* indexerParam = ParmVarDecl::Create(
    *m_context, nullptr, NoLoc, NoLoc, id, subscriptIndexType, nullptr, StorageClass::SC_None, nullptr);

  QualType T = TInfo->getType();
  DeclarationNameInfo NameInfo(functionTemplate->getDeclName(), NoLoc);
  CXXMethodDecl* method = CXXMethodDecl::Create(
    *m_context, dyn_cast<CXXRecordDecl>(owner), NoLoc, NameInfo, T, TInfo,
    SC_Extern, InlineSpecifiedFalse, IsConstexprFalse, NoLoc);

  // Add subscript attribute
  AddHLSLSubscriptAttr(method, *m_context, HLSubscriptOpcode::DefaultSubscript);

  // Record this function template specialization.
  method->setFunctionTemplateSpecialization(functionTemplate,
    TemplateArgumentList::CreateCopy(*m_context, &templateArgument, 1), 0);

  // Attach the parameters
  indexerParam->setOwningFunction(method);
  Proto.setParam(0, indexerParam);
  method->setParams(ArrayRef<ParmVarDecl*>(indexerParam));

  // Adjust access.
  method->setAccess(AccessSpecifier::AS_public);
  functionTemplate->setAccess(method->getAccess());

  return method;
}

/// <summary>
/// This routine combines Source into Target. If you have a symmetric operation 
/// and want to treat either side equally you should call it twice, swapping the
/// parameter order.
/// </summary>
static bool CombineObjectTypes(ArBasicKind Target, _In_ ArBasicKind Source,
                               _Out_opt_ ArBasicKind *pCombined) {
  if (Target == Source) {
    AssignOpt(Target, pCombined);
    return true;
  }

  if (Source == AR_OBJECT_NULL) {
    // NULL is valid for any object type.
    AssignOpt(Target, pCombined);
    return true;
  }

  switch (Target) {
  AR_BASIC_ROBJECT_CASES:
    if (Source == AR_OBJECT_STATEBLOCK) {
      AssignOpt(Target, pCombined);
      return true;
    }
    break;

  AR_BASIC_TEXTURE_CASES:

  AR_BASIC_NON_CMP_SAMPLER_CASES:
    if (Source == AR_OBJECT_SAMPLER || Source == AR_OBJECT_STATEBLOCK) {
      AssignOpt(Target, pCombined);
      return true;
    }
    break;

  case AR_OBJECT_SAMPLERCOMPARISON:
    if (Source == AR_OBJECT_STATEBLOCK) {
      AssignOpt(Target, pCombined);
      return true;
    }
    break;
  default:
    // Not a combinable target.
    break;
  }

  AssignOpt(AR_BASIC_UNKNOWN, pCombined);
  return false;
}

static ArBasicKind LiteralToConcrete(Expr *litExpr,
                                     HLSLExternalSource *pHLSLExternalSource) {
  if (IntegerLiteral *intLit = dyn_cast<IntegerLiteral>(litExpr)) {
    llvm::APInt val = intLit->getValue();
    unsigned width = val.getActiveBits();
    bool isNeg = val.isNegative();
    if (isNeg) {
      // Signed.
      if (width <= 32)
        return ArBasicKind::AR_BASIC_INT32;
      else
        return ArBasicKind::AR_BASIC_INT64;
    } else {
      // Unsigned.
      if (width <= 32)
        return ArBasicKind::AR_BASIC_UINT32;
      else
        return ArBasicKind::AR_BASIC_UINT64;
    }
  } else if (FloatingLiteral *floatLit = dyn_cast<FloatingLiteral>(litExpr)) {
    llvm::APFloat val = floatLit->getValue();
    unsigned width = val.getSizeInBits(val.getSemantics());
    if (width <= 16)
      return ArBasicKind::AR_BASIC_FLOAT16;
    else if (width <= 32)
      return ArBasicKind::AR_BASIC_FLOAT32;
    else
      return AR_BASIC_FLOAT64;
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(litExpr)) {
    ArBasicKind kind = LiteralToConcrete(UO->getSubExpr(), pHLSLExternalSource);
    if (UO->getOpcode() == UnaryOperator::Opcode::UO_Minus) {
      if (kind == ArBasicKind::AR_BASIC_UINT32)
        kind = ArBasicKind::AR_BASIC_INT32;
      else if (kind == ArBasicKind::AR_BASIC_UINT64)
        kind = ArBasicKind::AR_BASIC_INT64;
    }
    return kind;
  } else if (HLSLVectorElementExpr *VEE = dyn_cast<HLSLVectorElementExpr>(litExpr)) {
    return pHLSLExternalSource->GetTypeElementKind(VEE->getType());
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(litExpr)) {
    ArBasicKind kind = LiteralToConcrete(BO->getLHS(), pHLSLExternalSource);
    ArBasicKind kind1 = LiteralToConcrete(BO->getRHS(), pHLSLExternalSource);
    CombineBasicTypes(kind, kind1, &kind);
    return kind;
  } else if (ParenExpr *PE = dyn_cast<ParenExpr>(litExpr)) {
    ArBasicKind kind = LiteralToConcrete(PE->getSubExpr(), pHLSLExternalSource);
    return kind;
  } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(litExpr)) {
    ArBasicKind kind = LiteralToConcrete(CO->getLHS(), pHLSLExternalSource);
    ArBasicKind kind1 = LiteralToConcrete(CO->getRHS(), pHLSLExternalSource);
    CombineBasicTypes(kind, kind1, &kind);
    return kind;
  } else if (ImplicitCastExpr *IC = dyn_cast<ImplicitCastExpr>(litExpr)) {
    // Use target Type for cast.
    ArBasicKind kind = pHLSLExternalSource->GetTypeElementKind(IC->getType());
    return kind;
  } else {
    // Could only be function call.
    CallExpr *CE = cast<CallExpr>(litExpr);
    // TODO: calculate the function call result.
    if (CE->getNumArgs() == 1)
      return LiteralToConcrete(CE->getArg(0), pHLSLExternalSource);
    else {
      ArBasicKind kind = LiteralToConcrete(CE->getArg(0), pHLSLExternalSource);
      for (unsigned i = 1; i < CE->getNumArgs(); i++) {
        ArBasicKind kindI = LiteralToConcrete(CE->getArg(i), pHLSLExternalSource);
        CombineBasicTypes(kind, kindI, &kind);
      }
      return kind;
    }
  }
}

static bool SearchTypeInTable(ArBasicKind kind, const ArBasicKind *pCT) {
  while (AR_BASIC_UNKNOWN != *pCT && AR_BASIC_NOCAST != *pCT) {    
    if (kind == *pCT)
      return true;
    pCT++;
  }
  return false;
}

static ArBasicKind
ConcreteLiteralType(Expr *litExpr, ArBasicKind kind,
                    unsigned uLegalComponentTypes,
                    HLSLExternalSource *pHLSLExternalSource) {
  const ArBasicKind *pCT = g_LegalIntrinsicCompTypes[uLegalComponentTypes];
  ArBasicKind defaultKind = *pCT;
  // Use first none literal kind as defaultKind.
  while (AR_BASIC_UNKNOWN != *pCT && AR_BASIC_NOCAST != *pCT) {
    ArBasicKind kind = *pCT;
    pCT++;
    // Skip literal type.
    if (kind == AR_BASIC_LITERAL_INT || kind == AR_BASIC_LITERAL_FLOAT)
      continue;
    defaultKind = kind;
    break;
  }

  ArBasicKind litKind = LiteralToConcrete(litExpr, pHLSLExternalSource);

  if (kind == AR_BASIC_LITERAL_INT) {
    // Search for match first.
    // For literal arg which don't affect return type, the search should always success.
    // Unless use literal int on a float parameter.
    if (SearchTypeInTable(litKind, g_LegalIntrinsicCompTypes[uLegalComponentTypes]))
      return litKind;

    // Return the default.
    return defaultKind;
  }
  else {
    // Search for float32 first.
    if (SearchTypeInTable(AR_BASIC_FLOAT32, g_LegalIntrinsicCompTypes[uLegalComponentTypes]))
      return AR_BASIC_FLOAT32;
    // Search for float64.
    if (SearchTypeInTable(AR_BASIC_FLOAT64, g_LegalIntrinsicCompTypes[uLegalComponentTypes]))
      return AR_BASIC_FLOAT64;

    // return default.
    return defaultKind;
  }
}

_Use_decl_annotations_ bool
HLSLExternalSource::IsValidateObjectElement(const HLSL_INTRINSIC *pIntrinsic,
                                            QualType objectElement) {
  IntrinsicOp op = static_cast<IntrinsicOp>(pIntrinsic->Op);
  switch (op) {
  case IntrinsicOp::MOP_Sample:
  case IntrinsicOp::MOP_SampleBias:
  case IntrinsicOp::MOP_SampleCmp:
  case IntrinsicOp::MOP_SampleCmpLevelZero:
  case IntrinsicOp::MOP_SampleGrad:
  case IntrinsicOp::MOP_SampleLevel: {
    ArBasicKind kind = GetTypeElementKind(objectElement);
    UINT uBits = GET_BPROP_BITS(kind);
    return IS_BASIC_FLOAT(kind) && uBits != BPROP_BITS64;
  } break;
  default:
    return true;
  }
}

_Use_decl_annotations_
bool HLSLExternalSource::MatchArguments(
  const HLSL_INTRINSIC* pIntrinsic,
  QualType objectElement,
  QualType functionTemplateTypeArg,
  ArrayRef<Expr *> Args,
  QualType(&argTypes)[g_MaxIntrinsicParamCount + 1],
  size_t* argCount)
{
  DXASSERT_NOMSG(pIntrinsic != nullptr);
  DXASSERT_NOMSG(argCount != nullptr);

  static const UINT UnusedSize = 0xFF;
  static const BYTE MaxIntrinsicArgs = g_MaxIntrinsicParamCount + 1;
#define CAB(_) { if (!(_)) return false; }
  *argCount = 0;

  ArTypeObjectKind Template[MaxIntrinsicArgs];  // Template type for each argument, AR_TOBJ_UNKNOWN if unspecified.
  ArBasicKind ComponentType[MaxIntrinsicArgs];  // Component type for each argument, AR_BASIC_UNKNOWN if unspecified.
  UINT uSpecialSize[IA_SPECIAL_SLOTS];                // row/col matching types, UNUSED_INDEX32 if unspecified.

  // Reset infos
  std::fill(Template, Template + _countof(Template), AR_TOBJ_UNKNOWN);
  std::fill(ComponentType, ComponentType + _countof(ComponentType), AR_BASIC_UNKNOWN);
  std::fill(uSpecialSize, uSpecialSize + _countof(uSpecialSize), UnusedSize);
  
  const unsigned retArgIdx = 0;
  unsigned retTypeIdx = pIntrinsic->pArgs[retArgIdx].uComponentTypeId;

  // Populate the template for each argument.
  ArrayRef<Expr*>::iterator iterArg = Args.begin();
  ArrayRef<Expr*>::iterator end = Args.end();
  unsigned int iArg = 1;
  for (; iterArg != end; ++iterArg) {
    Expr* pCallArg = *iterArg;

    // No vararg support.
    if (iArg >= _countof(Template) || iArg > pIntrinsic->uNumArgs) {
      return false;
    }

    const HLSL_INTRINSIC_ARGUMENT *pIntrinsicArg;
    pIntrinsicArg = &pIntrinsic->pArgs[iArg];
    DXASSERT(pIntrinsicArg->uTemplateId != INTRIN_TEMPLATE_VARARGS, "no vararg support");

    QualType pType = pCallArg->getType();
    ArTypeObjectKind TypeInfoShapeKind = GetTypeObjectKind(pType);
    ArBasicKind TypeInfoEltKind = GetTypeElementKind(pType);

    if (pIntrinsicArg->uLegalComponentTypes == LICOMPTYPE_RAYDESC) {
      if (TypeInfoShapeKind == AR_TOBJ_COMPOUND) {
        if (CXXRecordDecl *pDecl = pType->getAsCXXRecordDecl()) {
          int index = FindObjectBasicKindIndex(pDecl);
          if (index != -1 && AR_OBJECT_RAY_DESC == g_ArBasicKindsAsTypes[index]) {
            ++iArg;
            continue;
          }
        }
      }
      m_sema->Diag(pCallArg->getExprLoc(),
                   diag::err_hlsl_ray_desc_required);
      return false;
    }

    if (pIntrinsicArg->uLegalComponentTypes == LICOMPTYPE_USER_DEFINED_TYPE) {
      DXASSERT(objectElement.isNull(), "");
      QualType Ty = pCallArg->getType();
      // Must be user define type for LICOMPTYPE_USER_DEFINED_TYPE arg.
      if (TypeInfoShapeKind != AR_TOBJ_COMPOUND) {
        m_sema->Diag(pCallArg->getExprLoc(),
                     diag::err_hlsl_no_struct_user_defined_type);
        return false;
      }
      objectElement = Ty;
      ++iArg;
      continue;
    }

    // If we are a type and templateID requires one, this isn't a match.
    if (pIntrinsicArg->uTemplateId == INTRIN_TEMPLATE_FROM_TYPE
      || pIntrinsicArg->uTemplateId == INTRIN_TEMPLATE_FROM_FUNCTION) {
      ++iArg;
      continue;
    }

    if (TypeInfoEltKind == AR_BASIC_LITERAL_INT ||
        TypeInfoEltKind == AR_BASIC_LITERAL_FLOAT) {
      bool affectRetType =
          (iArg != retArgIdx && retTypeIdx == pIntrinsicArg->uComponentTypeId);
      // For literal arg which don't affect return type, find concrete type.
      // For literal arg affect return type,
      //   TryEvalIntrinsic in CGHLSLMS.cpp will take care of cases
      //     where all argumentss are literal.
      //   CombineBasicTypes will cover the rest cases.
      if (!affectRetType) {
        TypeInfoEltKind = ConcreteLiteralType(
            pCallArg, TypeInfoEltKind, pIntrinsicArg->uLegalComponentTypes, this);
      }
    }

    UINT TypeInfoCols = 1;
    UINT TypeInfoRows = 1;
    switch (TypeInfoShapeKind) {
    case AR_TOBJ_MATRIX:
      GetRowsAndCols(pType, TypeInfoRows, TypeInfoCols);
      break;
    case AR_TOBJ_VECTOR:
      TypeInfoCols = GetHLSLVecSize(pType);
      break;
    case AR_TOBJ_BASIC:
    case AR_TOBJ_OBJECT:
      break;
    default:
      return false; // no struct, arrays or void
    }

    DXASSERT(
      pIntrinsicArg->uTemplateId < MaxIntrinsicArgs,
      "otherwise intrinsic table was modified and g_MaxIntrinsicParamCount was not updated (or uTemplateId is out of bounds)");

    // Compare template
    if ((AR_TOBJ_UNKNOWN == Template[pIntrinsicArg->uTemplateId]) ||
        ((AR_TOBJ_SCALAR == Template[pIntrinsicArg->uTemplateId]) &&
         (AR_TOBJ_VECTOR == TypeInfoShapeKind || AR_TOBJ_MATRIX == TypeInfoShapeKind))) {
      Template[pIntrinsicArg->uTemplateId] = TypeInfoShapeKind;
    }
    else if (AR_TOBJ_SCALAR == TypeInfoShapeKind) {
      if (AR_TOBJ_SCALAR != Template[pIntrinsicArg->uTemplateId] &&
          AR_TOBJ_VECTOR != Template[pIntrinsicArg->uTemplateId] &&
          AR_TOBJ_MATRIX != Template[pIntrinsicArg->uTemplateId]) {
        return false;
      }
    }
    else {
      if (TypeInfoShapeKind != Template[pIntrinsicArg->uTemplateId]) {
        return false;
      }
    }

    DXASSERT(
      pIntrinsicArg->uComponentTypeId < MaxIntrinsicArgs,
      "otherwise intrinsic table was modified and MaxIntrinsicArgs was not updated (or uComponentTypeId is out of bounds)");

    // Merge ComponentTypes
    if (AR_BASIC_UNKNOWN == ComponentType[pIntrinsicArg->uComponentTypeId]) {
      ComponentType[pIntrinsicArg->uComponentTypeId] = TypeInfoEltKind;
    }
    else {
      if (!CombineBasicTypes(
            ComponentType[pIntrinsicArg->uComponentTypeId],
            TypeInfoEltKind,
            &ComponentType[pIntrinsicArg->uComponentTypeId])) {
        return false;
      }
    }

    // Rows
    if (AR_TOBJ_SCALAR != TypeInfoShapeKind) {
      if (pIntrinsicArg->uRows >= IA_SPECIAL_BASE) {
        UINT uSpecialId = pIntrinsicArg->uRows - IA_SPECIAL_BASE;
        CAB(uSpecialId < IA_SPECIAL_SLOTS);
        if (uSpecialSize[uSpecialId] > TypeInfoRows) {
          uSpecialSize[uSpecialId] = TypeInfoRows;
        }
      }
      else {
        if (TypeInfoRows < pIntrinsicArg->uRows) {
          return false;
        }
      }
    }

    // Columns
    if (AR_TOBJ_SCALAR != TypeInfoShapeKind) {
      if (pIntrinsicArg->uCols >= IA_SPECIAL_BASE) {
        UINT uSpecialId = pIntrinsicArg->uCols - IA_SPECIAL_BASE;
        CAB(uSpecialId < IA_SPECIAL_SLOTS);

        if (uSpecialSize[uSpecialId] > TypeInfoCols) {
          uSpecialSize[uSpecialId] = TypeInfoCols;
        }
      }
      else {
        if (TypeInfoCols < pIntrinsicArg->uCols) {
          return false;
        }
      }
    }

    // Usage
    if (pIntrinsicArg->qwUsage & AR_QUAL_OUT) {
      if (pCallArg->getType().isConstQualified()) {
        // Can't use a const type in an out or inout parameter.
        return false;
      }
    }

    iArg++;
  }

  DXASSERT(iterArg == end, "otherwise the argument list wasn't fully processed");

  // Default template and component type for return value
  if (pIntrinsic->pArgs[0].qwUsage
    && pIntrinsic->pArgs[0].uTemplateId != INTRIN_TEMPLATE_FROM_TYPE
    && pIntrinsic->pArgs[0].uTemplateId != INTRIN_TEMPLATE_FROM_FUNCTION) {
    CAB(pIntrinsic->pArgs[0].uTemplateId < MaxIntrinsicArgs);
    if (AR_TOBJ_UNKNOWN == Template[pIntrinsic->pArgs[0].uTemplateId]) {
      Template[pIntrinsic->pArgs[0].uTemplateId] =
        g_LegalIntrinsicTemplates[pIntrinsic->pArgs[0].uLegalTemplates][0];

      if (pIntrinsic->pArgs[0].uComponentTypeId != INTRIN_COMPTYPE_FROM_TYPE_ELT0) {
        DXASSERT_NOMSG(pIntrinsic->pArgs[0].uComponentTypeId < MaxIntrinsicArgs);
        if (AR_BASIC_UNKNOWN == ComponentType[pIntrinsic->pArgs[0].uComponentTypeId]) {
          // half return type should map to float for min precision
          if (pIntrinsic->pArgs[0].uLegalComponentTypes ==
                  LEGAL_INTRINSIC_COMPTYPES::LICOMPTYPE_FLOAT16 &&
              getSema()->getLangOpts().UseMinPrecision) {
            ComponentType[pIntrinsic->pArgs[0].uComponentTypeId] =
              ArBasicKind::AR_BASIC_FLOAT32;
          }
          else {
            ComponentType[pIntrinsic->pArgs[0].uComponentTypeId] =
              g_LegalIntrinsicCompTypes[pIntrinsic->pArgs[0].uLegalComponentTypes][0];
          }
        }
      }
    }
  }

  // Make sure all template, component type, and texture type selections are valid.
  for (size_t i = 0; i < Args.size() + 1; i++) {
    const HLSL_INTRINSIC_ARGUMENT *pArgument = &pIntrinsic->pArgs[i];

    // Check template.
    if (pArgument->uTemplateId == INTRIN_TEMPLATE_FROM_TYPE
      || pArgument->uTemplateId == INTRIN_TEMPLATE_FROM_FUNCTION) {
      continue; // Already verified that this is available.
    }
    if (pArgument->uLegalComponentTypes == LICOMPTYPE_USER_DEFINED_TYPE) {
      continue;
    }

    const ArTypeObjectKind *pTT = g_LegalIntrinsicTemplates[pArgument->uLegalTemplates];
    if (AR_TOBJ_UNKNOWN != Template[i]) {
      if ((AR_TOBJ_SCALAR == Template[i]) && (AR_TOBJ_VECTOR == *pTT || AR_TOBJ_MATRIX == *pTT)) {
        Template[i] = *pTT;
      }
      else {
        while (AR_TOBJ_UNKNOWN != *pTT) {
          if (Template[i] == *pTT)
            break;
          pTT++;
        }
      }

      if (AR_TOBJ_UNKNOWN == *pTT)
        return false;
      }
    else if (pTT) {
      Template[i] = *pTT;
    }

    // Check component type.
    const ArBasicKind *pCT = g_LegalIntrinsicCompTypes[pArgument->uLegalComponentTypes];
    if (AR_BASIC_UNKNOWN != ComponentType[i]) {
      while (AR_BASIC_UNKNOWN != *pCT && AR_BASIC_NOCAST != *pCT) {
        if (ComponentType[i] == *pCT)
          break;
        pCT++;
      }

      // has to be a strict match
      if (*pCT == AR_BASIC_NOCAST)
        return false;

      // If it is an object, see if it can be cast to the first thing in the 
      // list, otherwise move on to next intrinsic.
      if (AR_TOBJ_OBJECT == Template[i] && AR_BASIC_UNKNOWN == *pCT) {
        if (!CombineObjectTypes(g_LegalIntrinsicCompTypes[pArgument->uLegalComponentTypes][0], ComponentType[i], nullptr)) {
          return false;
        }
      }

      if (AR_BASIC_UNKNOWN == *pCT) {
        ComponentType[i] = g_LegalIntrinsicCompTypes[pArgument->uLegalComponentTypes][0];
      }
    }
    else if (pCT) {
      ComponentType[i] = *pCT;
    }
  }

  // Default to a void return type.
  argTypes[0] = m_context->VoidTy;

  // Default specials sizes.
  for (UINT i = 0; i < IA_SPECIAL_SLOTS; i++) {
    if (UnusedSize == uSpecialSize[i]) {
      uSpecialSize[i] = 1;
    }
  }

  // Populate argTypes.
  for (size_t i = 0; i <= Args.size(); i++) {
    const HLSL_INTRINSIC_ARGUMENT *pArgument = &pIntrinsic->pArgs[i];

    if (!pArgument->qwUsage)
      continue;

    QualType pNewType;
    unsigned int quals = 0; // qualifications for this argument


    // If we have no type, set it to our input type (templatized)
    if (pArgument->uTemplateId == INTRIN_TEMPLATE_FROM_TYPE) {
      // Use the templated input type, but resize it if the
      // intrinsic's rows/cols isn't 0
      if (pArgument->uRows && pArgument->uCols) {
        UINT uRows, uCols = 0;

        // if type is overriden, use new type size, for
        // now it only supports scalars
        if (pArgument->uRows >= IA_SPECIAL_BASE) {
          UINT uSpecialId = pArgument->uRows - IA_SPECIAL_BASE;
          CAB(uSpecialId < IA_SPECIAL_SLOTS);

          uRows = uSpecialSize[uSpecialId];
        }
        else if (pArgument->uRows > 0) {
          uRows = pArgument->uRows;
        }

        if (pArgument->uCols >= IA_SPECIAL_BASE) {
          UINT uSpecialId = pArgument->uCols - IA_SPECIAL_BASE;
          CAB(uSpecialId < IA_SPECIAL_SLOTS);

          uCols = uSpecialSize[uSpecialId];
        }
        else if (pArgument->uCols > 0) {
          uCols = pArgument->uCols;
        }

        // 1x1 numeric outputs are always scalar.. since these
        // are most flexible
        if ((1 == uCols) && (1 == uRows)) {
          pNewType = objectElement;
          if (pNewType.isNull()) {
            return false;
          }
        }
        else {
          // non-scalars unsupported right now since nothing
          // uses it, would have to create either a type
          // list for sub-structures or just resize the
          // given type

          // VH(E_NOTIMPL);
          return false;
        }
      }
      else {
        DXASSERT_NOMSG(!pArgument->uRows && !pArgument->uCols);
        if (objectElement.isNull()) {
          return false;
        }
        pNewType = objectElement;
      }
    }
    else if (pArgument->uTemplateId == INTRIN_TEMPLATE_FROM_FUNCTION) {
      if (functionTemplateTypeArg.isNull()) {
        if (i == 0) {
          // [RW]ByteAddressBuffer.Load, default to uint
          pNewType = m_context->UnsignedIntTy;
        }
        else {
          // [RW]ByteAddressBuffer.Store, default to argument type
          pNewType = Args[i - 1]->getType().getNonReferenceType();
          if (const BuiltinType *BuiltinTy = pNewType->getAs<BuiltinType>()) {
            // For backcompat, ensure that Store(0, 42 or 42.0) matches a uint/float overload
            // rather than a uint64_t/double one.
            if (BuiltinTy->getKind() == BuiltinType::LitInt) {
              pNewType = m_context->UnsignedIntTy;
            } else if (BuiltinTy->getKind() == BuiltinType::LitFloat) {
              pNewType = m_context->FloatTy;
            }
          }
        }
      }
      else {
        pNewType = functionTemplateTypeArg;
      }
    }
    else if (pArgument->uLegalComponentTypes == LICOMPTYPE_USER_DEFINED_TYPE) {
      if (objectElement.isNull()) {
        return false;
      }
      pNewType = objectElement;
    } else {
      ArBasicKind pEltType;

      // ComponentType, if the Id is special then it gets the
      // component type from the first component of the type, if
      // we need more (for the second component, e.g.), then we
      // can use more specials, etc.
      if (pArgument->uComponentTypeId == INTRIN_COMPTYPE_FROM_TYPE_ELT0) {
        if (objectElement.isNull()) {
          return false;
        }
        pEltType = GetTypeElementKind(objectElement);
        if (!IsValidBasicKind(pEltType)) {
          // This can happen with Texture2D<Struct> or other invalid declarations
          return false;
        }
      }
      else {
        pEltType = ComponentType[pArgument->uComponentTypeId];
        DXASSERT_VALIDBASICKIND(pEltType);
      }

      UINT uRows, uCols;

      // Rows
      if (pArgument->uRows >= IA_SPECIAL_BASE) {
        UINT uSpecialId = pArgument->uRows - IA_SPECIAL_BASE;
        CAB(uSpecialId < IA_SPECIAL_SLOTS);
        uRows = uSpecialSize[uSpecialId];
      }
      else {
        uRows = pArgument->uRows;
      }

      // Cols
      if (pArgument->uCols >= IA_SPECIAL_BASE) {
        UINT uSpecialId = pArgument->uCols - IA_SPECIAL_BASE;
        CAB(uSpecialId < IA_SPECIAL_SLOTS);
        uCols = uSpecialSize[uSpecialId];
      }
      else {
        uCols = pArgument->uCols;
      }

      // Verify that the final results are in bounds.
      CAB(uCols > 0 && uCols <= MaxVectorSize && uRows > 0 && uRows <= MaxVectorSize);

      // Const
      UINT64 qwQual = pArgument->qwUsage & (AR_QUAL_ROWMAJOR | AR_QUAL_COLMAJOR);

      if ((0 == i) || !(pArgument->qwUsage & AR_QUAL_OUT))
        qwQual |= AR_QUAL_CONST;

      DXASSERT_VALIDBASICKIND(pEltType);
      pNewType = NewSimpleAggregateType(Template[pArgument->uTemplateId], pEltType, qwQual, uRows, uCols);
    }

    DXASSERT(!pNewType.isNull(), "otherwise there's a branch in this function that fails to assign this");
    argTypes[i] = QualType(pNewType.getTypePtr(), quals);

    // TODO: support out modifier
    //if (pArgument->qwUsage & AR_QUAL_OUT) {
    //  argTypes[i] = m_context->getLValueReferenceType(argTypes[i].withConst());
    //}
  }

  *argCount = iArg;
  DXASSERT(
    *argCount == pIntrinsic->uNumArgs,
    "In the absence of varargs, a successful match would indicate we have as many arguments and types as the intrinsic template");
  return true;
#undef CAB
}

_Use_decl_annotations_
HLSLExternalSource::FindStructBasicTypeResult
HLSLExternalSource::FindStructBasicType(DeclContext* functionDeclContext)
{
  DXASSERT_NOMSG(functionDeclContext != nullptr);

  // functionDeclContext may be a specialization of a template, such as AppendBuffer<MY_STRUCT>, or it
  // may be a simple class, such as RWByteAddressBuffer.
  const CXXRecordDecl* recordDecl = GetRecordDeclForBuiltInOrStruct(functionDeclContext);

  // We save the caller from filtering out other types of context (like the translation unit itself).
  if (recordDecl != nullptr)
  {
    int index = FindObjectBasicKindIndex(recordDecl);
    if (index != -1) {
      ArBasicKind kind = g_ArBasicKindsAsTypes[index];
      return HLSLExternalSource::FindStructBasicTypeResult(kind, index);
    }
  }

  return HLSLExternalSource::FindStructBasicTypeResult(AR_BASIC_UNKNOWN, 0);
}

_Use_decl_annotations_
void HLSLExternalSource::FindIntrinsicTable(DeclContext* functionDeclContext, const char** name, const HLSL_INTRINSIC** intrinsics, size_t* intrinsicCount)
{
  DXASSERT_NOMSG(functionDeclContext != nullptr);
  DXASSERT_NOMSG(name != nullptr);
  DXASSERT_NOMSG(intrinsics != nullptr);
  DXASSERT_NOMSG(intrinsicCount != nullptr);

  *intrinsics = nullptr;
  *intrinsicCount = 0;
  *name = nullptr;

  HLSLExternalSource::FindStructBasicTypeResult lookup = FindStructBasicType(functionDeclContext);
  if (lookup.Found()) {
    GetIntrinsicMethods(lookup.Kind, intrinsics, intrinsicCount);
    *name = g_ArBasicTypeNames[lookup.Kind];
  }
}

static bool BinaryOperatorKindIsArithmetic(BinaryOperatorKind Opc)
{
  return
    // Arithmetic operators.
    Opc == BinaryOperatorKind::BO_Add ||
    Opc == BinaryOperatorKind::BO_AddAssign ||
    Opc == BinaryOperatorKind::BO_Sub ||
    Opc == BinaryOperatorKind::BO_SubAssign ||
    Opc == BinaryOperatorKind::BO_Rem ||
    Opc == BinaryOperatorKind::BO_RemAssign ||
    Opc == BinaryOperatorKind::BO_Div ||
    Opc == BinaryOperatorKind::BO_DivAssign ||
    Opc == BinaryOperatorKind::BO_Mul ||
    Opc == BinaryOperatorKind::BO_MulAssign;
}

static bool BinaryOperatorKindIsCompoundAssignment(BinaryOperatorKind Opc)
{
  return
    // Arithmetic-and-assignment operators.
    Opc == BinaryOperatorKind::BO_AddAssign ||
    Opc == BinaryOperatorKind::BO_SubAssign ||
    Opc == BinaryOperatorKind::BO_RemAssign ||
    Opc == BinaryOperatorKind::BO_DivAssign ||
    Opc == BinaryOperatorKind::BO_MulAssign ||
    // Bitwise-and-assignment operators.
    Opc == BinaryOperatorKind::BO_ShlAssign ||
    Opc == BinaryOperatorKind::BO_ShrAssign ||
    Opc == BinaryOperatorKind::BO_AndAssign ||
    Opc == BinaryOperatorKind::BO_OrAssign ||
    Opc == BinaryOperatorKind::BO_XorAssign;
}

static bool BinaryOperatorKindIsCompoundAssignmentForBool(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_AndAssign ||
    Opc == BinaryOperatorKind::BO_OrAssign ||
    Opc == BinaryOperatorKind::BO_XorAssign;
}

static bool BinaryOperatorKindIsBitwise(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_Shl ||
    Opc == BinaryOperatorKind::BO_ShlAssign ||
    Opc == BinaryOperatorKind::BO_Shr ||
    Opc == BinaryOperatorKind::BO_ShrAssign ||
    Opc == BinaryOperatorKind::BO_And ||
    Opc == BinaryOperatorKind::BO_AndAssign ||
    Opc == BinaryOperatorKind::BO_Or ||
    Opc == BinaryOperatorKind::BO_OrAssign ||
    Opc == BinaryOperatorKind::BO_Xor ||
    Opc == BinaryOperatorKind::BO_XorAssign;
}

static bool BinaryOperatorKindIsBitwiseShift(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_Shl ||
    Opc == BinaryOperatorKind::BO_ShlAssign ||
    Opc == BinaryOperatorKind::BO_Shr ||
    Opc == BinaryOperatorKind::BO_ShrAssign;
}

static bool BinaryOperatorKindIsEqualComparison(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_EQ ||
    Opc == BinaryOperatorKind::BO_NE;
}

static bool BinaryOperatorKindIsOrderComparison(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_LT ||
    Opc == BinaryOperatorKind::BO_GT ||
    Opc == BinaryOperatorKind::BO_LE ||
    Opc == BinaryOperatorKind::BO_GE;
}

static bool BinaryOperatorKindIsComparison(BinaryOperatorKind Opc)
{
  return BinaryOperatorKindIsEqualComparison(Opc) || BinaryOperatorKindIsOrderComparison(Opc);
}

static bool BinaryOperatorKindIsLogical(BinaryOperatorKind Opc)
{
  return
    Opc == BinaryOperatorKind::BO_LAnd ||
    Opc == BinaryOperatorKind::BO_LOr;
}

static bool BinaryOperatorKindRequiresNumeric(BinaryOperatorKind Opc)
{
  return
    BinaryOperatorKindIsArithmetic(Opc) ||
    BinaryOperatorKindIsOrderComparison(Opc) ||
    BinaryOperatorKindIsLogical(Opc);
}

static bool BinaryOperatorKindRequiresIntegrals(BinaryOperatorKind Opc)
{
  return BinaryOperatorKindIsBitwise(Opc);
}

static bool BinaryOperatorKindRequiresBoolAsNumeric(BinaryOperatorKind Opc)
{
  return
    BinaryOperatorKindIsBitwise(Opc) ||
    BinaryOperatorKindIsArithmetic(Opc);
}

static bool UnaryOperatorKindRequiresIntegrals(UnaryOperatorKind Opc)
{
  return Opc == UnaryOperatorKind::UO_Not;
}

static bool UnaryOperatorKindRequiresNumerics(UnaryOperatorKind Opc)
{
  return
    Opc == UnaryOperatorKind::UO_LNot ||
    Opc == UnaryOperatorKind::UO_Plus ||
    Opc == UnaryOperatorKind::UO_Minus ||
    // The omission in fxc caused objects and structs to accept this.
    Opc == UnaryOperatorKind::UO_PreDec || Opc == UnaryOperatorKind::UO_PreInc ||
    Opc == UnaryOperatorKind::UO_PostDec || Opc == UnaryOperatorKind::UO_PostInc;
}

static bool UnaryOperatorKindRequiresModifiableValue(UnaryOperatorKind Opc)
{
  return
    Opc == UnaryOperatorKind::UO_PreDec || Opc == UnaryOperatorKind::UO_PreInc ||
    Opc == UnaryOperatorKind::UO_PostDec || Opc == UnaryOperatorKind::UO_PostInc;
}

static bool UnaryOperatorKindRequiresBoolAsNumeric(UnaryOperatorKind Opc)
{
  return
    Opc == UnaryOperatorKind::UO_Not ||
    Opc == UnaryOperatorKind::UO_Plus ||
    Opc == UnaryOperatorKind::UO_Minus;
}

static bool UnaryOperatorKindDisallowsBool(UnaryOperatorKind Opc)
{
  return
    Opc == UnaryOperatorKind::UO_PreDec || Opc == UnaryOperatorKind::UO_PreInc ||
    Opc == UnaryOperatorKind::UO_PostDec || Opc == UnaryOperatorKind::UO_PostInc;
}

static bool IsIncrementOp(UnaryOperatorKind Opc) {
  return Opc == UnaryOperatorKind::UO_PreInc || Opc == UnaryOperatorKind::UO_PostInc;
}

/// <summary>
/// Checks whether the specified AR_TOBJ* value is a primitive or aggregate of primitive elements
/// (as opposed to a built-in object like a sampler or texture, or a void type).
/// </summary>
static bool IsObjectKindPrimitiveAggregate(ArTypeObjectKind value)
{
  return
    value == AR_TOBJ_BASIC ||
    value == AR_TOBJ_MATRIX ||
    value == AR_TOBJ_VECTOR;
}

static bool IsBasicKindIntegral(ArBasicKind value)
{
  return IS_BASIC_AINT(value) || IS_BASIC_BOOL(value);
}

static bool IsBasicKindIntMinPrecision(ArBasicKind kind)
{
  return IS_BASIC_SINT(kind) && IS_BASIC_MIN_PRECISION(kind);
}

static bool IsBasicKindNumeric(ArBasicKind value)
{
  return GetBasicKindProps(value) & BPROP_NUMERIC;
}

ExprResult HLSLExternalSource::PromoteToIntIfBool(ExprResult& E)
{
  // An invalid expression is pass-through at this point.
  if (E.isInvalid())
  {
    return E;
  }

  QualType qt = E.get()->getType();
  ArBasicKind elementKind = this->GetTypeElementKind(qt);
  if (elementKind != AR_BASIC_BOOL)
  {
    return E;
  }

  // Construct a scalar/vector/matrix type with the same shape as E.
  ArTypeObjectKind objectKind = this->GetTypeObjectKind(qt);

  QualType targetType;
  UINT colCount, rowCount;
  GetRowsAndColsForAny(qt, rowCount, colCount);

  targetType = NewSimpleAggregateType(objectKind, AR_BASIC_INT32, 0, rowCount, colCount)->getCanonicalTypeInternal();

  if (E.get()->isLValue()) {
    E = m_sema->DefaultLvalueConversion(E.get()).get();
  }

  switch (objectKind)
  {
  case AR_TOBJ_SCALAR:
    return ImplicitCastExpr::Create(*m_context, targetType, CastKind::CK_IntegralCast, E.get(), nullptr, ExprValueKind::VK_RValue);
  case AR_TOBJ_ARRAY:
  case AR_TOBJ_VECTOR:
  case AR_TOBJ_MATRIX:
    return ImplicitCastExpr::Create(*m_context, targetType, CastKind::CK_HLSLCC_IntegralCast, E.get(), nullptr, ExprValueKind::VK_RValue);
  default:
    DXASSERT(false, "unsupported objectKind for PromoteToIntIfBool");
  }
  return E;
}

_Use_decl_annotations_
void HLSLExternalSource::CollectInfo(QualType type, ArTypeInfo* pTypeInfo)
{
  DXASSERT_NOMSG(pTypeInfo != nullptr);
  DXASSERT_NOMSG(!type.isNull());

  memset(pTypeInfo, 0, sizeof(*pTypeInfo));

  // TODO: Get* functions used here add up to a bunch of redundant code.
  //       Try to inline that here, making it cheaper to use this function
  //       when retrieving multiple properties.
  pTypeInfo->ObjKind = GetTypeElementKind(type);
  pTypeInfo->EltKind = pTypeInfo->ObjKind;
  pTypeInfo->ShapeKind = GetTypeObjectKind(type);
  GetRowsAndColsForAny(type, pTypeInfo->uRows, pTypeInfo->uCols);
  pTypeInfo->uTotalElts = pTypeInfo->uRows * pTypeInfo->uCols;
}

// Highest possible score (i.e., worst possible score).
static const UINT64 SCORE_MAX = 0xFFFFFFFFFFFFFFFF;

// Leave the first two score bits to handle higher-level
// variations like target type.
#define SCORE_MIN_SHIFT 2

// Space out scores to allow up to 128 parameters to
// vary between score sets spill into each other.
#define SCORE_PARAM_SHIFT 7

unsigned HLSLExternalSource::GetNumElements(QualType anyType) {
  if (anyType.isNull()) {
    return 0;
  }

  anyType = GetStructuralForm(anyType);

  ArTypeObjectKind kind = GetTypeObjectKind(anyType);
  switch (kind) {
  case AR_TOBJ_BASIC:
  case AR_TOBJ_OBJECT:
  case AR_TOBJ_STRING:
    return 1;
  case AR_TOBJ_COMPOUND: {
    // TODO: consider caching this value for perf
    unsigned total = 0;
    const RecordType *recordType = anyType->getAs<RecordType>();
    RecordDecl::field_iterator fi = recordType->getDecl()->field_begin();
    RecordDecl::field_iterator fend = recordType->getDecl()->field_end();
    while (fi != fend) {
      total += GetNumElements(fi->getType());
      ++fi;
    }
    return total;
  }
  case AR_TOBJ_ARRAY:
  case AR_TOBJ_MATRIX:
  case AR_TOBJ_VECTOR:
    return GetElementCount(anyType);
  default:
    DXASSERT(kind == AR_TOBJ_VOID,
             "otherwise the type cannot be classified or is not supported");
    return 0;
  }
}

unsigned HLSLExternalSource::GetNumBasicElements(QualType anyType) {
  if (anyType.isNull()) {
    return 0;
  }

  anyType = GetStructuralForm(anyType);

  ArTypeObjectKind kind = GetTypeObjectKind(anyType);
  switch (kind) {
  case AR_TOBJ_BASIC:
  case AR_TOBJ_OBJECT:
  case AR_TOBJ_STRING:
    return 1;
  case AR_TOBJ_COMPOUND: {
    // TODO: consider caching this value for perf
    unsigned total = 0;
    const RecordType *recordType = anyType->getAs<RecordType>();
    RecordDecl * RD = recordType->getDecl();
    // Take care base.
    if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
      if (CXXRD->getNumBases()) {
        for (const auto &I : CXXRD->bases()) {
          const CXXRecordDecl *BaseDecl =
              cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
          if (BaseDecl->field_empty())
            continue;
          QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
          total += GetNumBasicElements(parentTy);
        }
      }
    }
    RecordDecl::field_iterator fi = RD->field_begin();
    RecordDecl::field_iterator fend = RD->field_end();
    while (fi != fend) {
      total += GetNumBasicElements(fi->getType());
      ++fi;
    }
    return total;
  }
  case AR_TOBJ_ARRAY: {
    unsigned arraySize = GetElementCount(anyType);
    unsigned eltSize = GetNumBasicElements(
        QualType(anyType->getArrayElementTypeNoTypeQual(), 0));
    return arraySize * eltSize;
  }
  case AR_TOBJ_MATRIX:
  case AR_TOBJ_VECTOR:
    return GetElementCount(anyType);
  default:
    DXASSERT(kind == AR_TOBJ_VOID,
             "otherwise the type cannot be classified or is not supported");
    return 0;
  }
}

unsigned HLSLExternalSource::GetNumConvertCheckElts(QualType leftType,
                                                    unsigned leftSize,
                                                    QualType rightType,
                                                    unsigned rightSize) {
  // We can convert from a larger type to a smaller
  // but not a smaller type to a larger so default
  // to just comparing the destination size.
  unsigned uElts = leftSize;

  leftType = GetStructuralForm(leftType);
  rightType = GetStructuralForm(rightType);

  if (leftType->isArrayType() && rightType->isArrayType()) {
    //
    // If we're comparing arrays we don't
    // need to compare every element of
    // the arrays since all elements
    // will have the same type.
    // We only need to compare enough
    // elements that we've tried every
    // possible mix of dst and src elements.
    //

    // TODO: handle multidimensional arrays and arrays of arrays
    QualType pDstElt = leftType->getAsArrayTypeUnsafe()->getElementType();
    unsigned uDstEltSize = GetNumElements(pDstElt);

    QualType pSrcElt = rightType->getAsArrayTypeUnsafe()->getElementType();
    unsigned uSrcEltSize = GetNumElements(pSrcElt);

    if (uDstEltSize == uSrcEltSize) {
      uElts = uDstEltSize;
    } else if (uDstEltSize > uSrcEltSize) {
      // If one size is not an even multiple of the other we need to let the
      // full compare run in order to try all alignments.
      if (uSrcEltSize && (uDstEltSize % uSrcEltSize) == 0) {
        uElts = uDstEltSize;
      }
    } else if (uDstEltSize && (uSrcEltSize % uDstEltSize) == 0) {
      uElts = uSrcEltSize;
    }
  }

  return uElts;
}

QualType HLSLExternalSource::GetNthElementType(QualType type, unsigned index) {
  if (type.isNull()) {
    return type;
  }

  ArTypeObjectKind kind = GetTypeObjectKind(type);
  switch (kind) {
  case AR_TOBJ_BASIC:
  case AR_TOBJ_OBJECT:
  case AR_TOBJ_STRING:
    return (index == 0) ? type : QualType();
  case AR_TOBJ_COMPOUND: {
    // TODO: consider caching this value for perf
    const RecordType *recordType = type->getAsStructureType();
    RecordDecl::field_iterator fi = recordType->getDecl()->field_begin();
    RecordDecl::field_iterator fend = recordType->getDecl()->field_end();
    while (fi != fend) {
      if (!fi->getType().isNull()) {
        unsigned subElements = GetNumElements(fi->getType());
        if (index < subElements) {
          return GetNthElementType(fi->getType(), index);
        } else {
          index -= subElements;
        }
      }
      ++fi;
    }
    return QualType();
  }
  case AR_TOBJ_ARRAY: {
    unsigned arraySize;
    QualType elementType;
    unsigned elementCount;
    elementType = type.getNonReferenceType()->getAsArrayTypeUnsafe()->getElementType();
    elementCount = GetElementCount(elementType);
    if (index < elementCount) {
      return GetNthElementType(elementType, index);
    }
    arraySize = GetArraySize(type);
    if (index >= arraySize * elementCount) {
      return QualType();
    }
    return GetNthElementType(elementType, index % elementCount);
  }
  case AR_TOBJ_MATRIX:
  case AR_TOBJ_VECTOR:
    return (index < GetElementCount(type)) ? GetMatrixOrVectorElementType(type)
               : QualType();
  default:
    DXASSERT(kind == AR_TOBJ_VOID,
             "otherwise the type cannot be classified or is not supported");
    return QualType();
  }
}

bool HLSLExternalSource::IsPromotion(ArBasicKind leftKind, ArBasicKind rightKind) {
  // Eliminate exact matches first, then check for promotions.
  if (leftKind == rightKind) {
    return false;
  }

  switch (rightKind) {
  case AR_BASIC_FLOAT16:
    switch (leftKind) {
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    case AR_BASIC_FLOAT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    switch (leftKind) {
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_FLOAT32:
    switch (leftKind) {
    case AR_BASIC_FLOAT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_MIN10FLOAT:
    switch (leftKind) {
    case AR_BASIC_MIN16FLOAT:
    case AR_BASIC_FLOAT16:
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    case AR_BASIC_FLOAT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_MIN16FLOAT:
    switch (leftKind) {
    case AR_BASIC_FLOAT16:
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    case AR_BASIC_FLOAT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;

  case AR_BASIC_INT8:
  case AR_BASIC_UINT8:
    // For backwards compat we consider signed/unsigned the same.
    switch (leftKind) {
    case AR_BASIC_INT16:
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
    case AR_BASIC_UINT16:
    case AR_BASIC_UINT32:
    case AR_BASIC_UINT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_INT16:
  case AR_BASIC_UINT16:
    // For backwards compat we consider signed/unsigned the same.
    switch (leftKind) {
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
    case AR_BASIC_UINT32:
    case AR_BASIC_UINT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_INT32:
  case AR_BASIC_UINT32:
    // For backwards compat we consider signed/unsigned the same.
    switch (leftKind) {
    case AR_BASIC_INT64:
    case AR_BASIC_UINT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_MIN12INT:
    switch (leftKind) {
    case AR_BASIC_MIN16INT:
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_MIN16INT:
    switch (leftKind) {
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  case AR_BASIC_MIN16UINT:
    switch (leftKind) {
    case AR_BASIC_UINT32:
    case AR_BASIC_UINT64:
      return true;
    default:
      return false; // No other type is a promotion.
    }
    break;
  }

  return false;
}

bool HLSLExternalSource::IsCast(ArBasicKind leftKind, ArBasicKind rightKind) {
  // Eliminate exact matches first, then check for casts.
  if (leftKind == rightKind) {
    return false;
  }

  //
  // All minimum-bits types are only considered matches of themselves
  // and thus are not in this table.
  //

  switch (leftKind) {
  case AR_BASIC_LITERAL_INT:
    switch (rightKind) {
    case AR_BASIC_INT8:
    case AR_BASIC_INT16:
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
    case AR_BASIC_UINT8:
    case AR_BASIC_UINT16:
    case AR_BASIC_UINT32:
    case AR_BASIC_UINT64:
      return false;
    default:
      break; // No other valid cast types
    }
    break;

  case AR_BASIC_INT8:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_UINT8:
      return false;
    default:
      break; // No other valid cast types
    }
    break;

  case AR_BASIC_INT16:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_UINT16:
      return false;
    default:
      break; // No other valid cast types
    }
    break;

  case AR_BASIC_INT32:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_UINT32:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_INT64:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_UINT64:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_UINT8:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_INT8:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_UINT16:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_INT16:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_UINT32:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_INT32:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_UINT64:
    switch (rightKind) {
    // For backwards compat we consider signed/unsigned the same.
    case AR_BASIC_LITERAL_INT:
    case AR_BASIC_INT64:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_LITERAL_FLOAT:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
    case AR_BASIC_FLOAT16:
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    case AR_BASIC_FLOAT64:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_FLOAT16:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_FLOAT32:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;

  case AR_BASIC_FLOAT64:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
      return false;
    default:
      break; // No other valid cast types.
    }
    break;
  default:
    break; // No other relevant targets.
  }

  return true;
}

bool HLSLExternalSource::IsIntCast(ArBasicKind leftKind, ArBasicKind rightKind) {
  // Eliminate exact matches first, then check for casts.
  if (leftKind == rightKind) {
    return false;
  }

  //
  // All minimum-bits types are only considered matches of themselves
  // and thus are not in this table.
  //

  switch (leftKind) {
  case AR_BASIC_LITERAL_INT:
    switch (rightKind) {
    case AR_BASIC_INT8:
    case AR_BASIC_INT16:
    case AR_BASIC_INT32:
    case AR_BASIC_INT64:
    case AR_BASIC_UINT8:
    case AR_BASIC_UINT16:
    case AR_BASIC_UINT32:
    case AR_BASIC_UINT64:
      return false;
    default:
      break; // No other valid conversions
    }
    break;

  case AR_BASIC_INT8:
  case AR_BASIC_INT16:
  case AR_BASIC_INT32:
  case AR_BASIC_INT64:
  case AR_BASIC_UINT8:
  case AR_BASIC_UINT16:
  case AR_BASIC_UINT32:
  case AR_BASIC_UINT64:
    switch (rightKind) {
    case AR_BASIC_LITERAL_INT:
      return false;
    default:
      break; // No other valid conversions
    }
    break;

  case AR_BASIC_LITERAL_FLOAT:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
    case AR_BASIC_FLOAT16:
    case AR_BASIC_FLOAT32:
    case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
    case AR_BASIC_FLOAT64:
      return false;
    default:
      break; // No other valid conversions
    }
    break;

  case AR_BASIC_FLOAT16:
  case AR_BASIC_FLOAT32:
  case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
  case AR_BASIC_FLOAT64:
    switch (rightKind) {
    case AR_BASIC_LITERAL_FLOAT:
      return false;
    default:
      break; // No other valid conversions
    }
    break;
  default:
    // No other relevant targets
    break;
  }

  return true;
}

UINT64 HLSLExternalSource::ScoreCast(QualType pLType, QualType pRType)
{
  if (pLType.getCanonicalType() == pRType.getCanonicalType()) {
    return 0;
  }

  UINT64 uScore = 0;
  UINT uLSize = GetNumElements(pLType);
  UINT uRSize = GetNumElements(pRType);
  UINT uCompareSize;

  bool bLCast = false;
  bool bRCast = false;
  bool bLIntCast = false;
  bool bRIntCast = false;
  bool bLPromo = false;
  bool bRPromo = false;

  uCompareSize = GetNumConvertCheckElts(pLType, uLSize, pRType, uRSize);
  if (uCompareSize > uRSize) {
    uCompareSize = uRSize;
  }

  for (UINT i = 0; i < uCompareSize; i++) {
    ArBasicKind LeftElementKind, RightElementKind;
    ArBasicKind CombinedKind = AR_BASIC_BOOL;

    QualType leftSub = GetNthElementType(pLType, i);
    QualType rightSub = GetNthElementType(pRType, i);
    ArTypeObjectKind leftKind = GetTypeObjectKind(leftSub);
    ArTypeObjectKind rightKind = GetTypeObjectKind(rightSub);
    LeftElementKind = GetTypeElementKind(leftSub);
    RightElementKind = GetTypeElementKind(rightSub);

    // CollectInfo is called with AR_TINFO_ALLOW_OBJECTS, and the resulting
    // information needed is the ShapeKind, EltKind and ObjKind.

    if (!leftSub.isNull() && !rightSub.isNull() && leftKind != AR_TOBJ_INVALID && rightKind != AR_TOBJ_INVALID) {
      bool bCombine;
      
      if (leftKind == AR_TOBJ_OBJECT || rightKind == AR_TOBJ_OBJECT) {
        DXASSERT(rightKind == AR_TOBJ_OBJECT, "otherwise prior check is incorrect");
        ArBasicKind LeftObjKind = LeftElementKind; // actually LeftElementKind would have been the element
        ArBasicKind RightObjKind = RightElementKind;
        LeftElementKind = LeftObjKind;
        RightElementKind = RightObjKind;

        if (leftKind != rightKind) {
          bCombine = false;
        }
        else if (!(bCombine = CombineObjectTypes(LeftObjKind, RightObjKind, &CombinedKind))) {
          bCombine = CombineObjectTypes(RightObjKind, LeftObjKind, &CombinedKind);
        }
      }
      else {
        bCombine = CombineBasicTypes(LeftElementKind, RightElementKind, &CombinedKind);
      }

      if (bCombine && IsPromotion(LeftElementKind, CombinedKind)) {
        bLPromo = true;
      }
      else if (!bCombine || IsCast(LeftElementKind, CombinedKind)) {
        bLCast = true;
      }
      else if (IsIntCast(LeftElementKind, CombinedKind)) {
        bLIntCast = true;
      }

      if (bCombine && IsPromotion(CombinedKind, RightElementKind)) {
        bRPromo = true;
      } else if (!bCombine || IsCast(CombinedKind, RightElementKind)) {
        bRCast = true;
      } else if (IsIntCast(CombinedKind, RightElementKind)) {
        bRIntCast = true;
      }
    } else {
      bLCast = true;
      bRCast = true;
    }
  }

#define SCORE_COND(shift, cond) { \
  if (cond) uScore += 1ULL << (SCORE_MIN_SHIFT + SCORE_PARAM_SHIFT * shift); }
  SCORE_COND(0, uRSize < uLSize);
  SCORE_COND(1, bLPromo);
  SCORE_COND(2, bRPromo);
  SCORE_COND(3, bLIntCast);
  SCORE_COND(4, bRIntCast);
  SCORE_COND(5, bLCast);
  SCORE_COND(6, bRCast);
  SCORE_COND(7, uLSize < uRSize);
#undef SCORE_COND

  // Make sure our scores fit in a UINT64.
  C_ASSERT(SCORE_MIN_SHIFT + SCORE_PARAM_SHIFT * 8 <= 64);

  return uScore;
}

UINT64 HLSLExternalSource::ScoreImplicitConversionSequence(const ImplicitConversionSequence *ics) {
  DXASSERT(ics, "otherwise conversion has not been initialized");
  if (!ics->isInitialized()) {
    return 0;
  }
  if (!ics->isStandard()) {
    return SCORE_MAX;
  }

  QualType fromType = ics->Standard.getFromType();
  QualType toType = ics->Standard.getToType(2); // final type
  return ScoreCast(toType, fromType);
}

UINT64 HLSLExternalSource::ScoreFunction(OverloadCandidateSet::iterator &Cand) {
  // Ignore target version mismatches.

  // in/out considerations have been taken care of by viability.

  // 'this' considerations don't matter without inheritance, other
  // than lookup and viability.

  UINT64 result = 0;
  for (unsigned convIdx = 0; convIdx < Cand->NumConversions; ++convIdx) {
    UINT64 score;
    
    score = ScoreImplicitConversionSequence(Cand->Conversions + convIdx);
    if (score == SCORE_MAX) {
      return SCORE_MAX;
    }
    result += score;

    score = ScoreImplicitConversionSequence(Cand->OutConversions + convIdx);
    if (score == SCORE_MAX) {
      return SCORE_MAX;
    }
    result += score;
  }
  return result;
}

OverloadingResult HLSLExternalSource::GetBestViableFunction(
  SourceLocation Loc,
  OverloadCandidateSet& set,
  OverloadCandidateSet::iterator& Best)
{
  UINT64 bestScore = SCORE_MAX;
  unsigned scoreMatch = 0;
  Best = set.end();

  if (set.size() == 1 && set.begin()->Viable) {
    Best = set.begin();
    return OR_Success;
  }

  for (OverloadCandidateSet::iterator Cand = set.begin(); Cand != set.end(); ++Cand) {
    if (Cand->Viable) {
      UINT64 score = ScoreFunction(Cand);
      if (score != SCORE_MAX) {
        if (score == bestScore) {
          ++scoreMatch;
        } else if (score < bestScore) {
          Best = Cand;
          scoreMatch = 1;
          bestScore = score;
        }
      }
    }
  }

  if (Best == set.end()) {
    return OR_No_Viable_Function;
  }

  if (scoreMatch > 1) {
    Best = set.end();
    return OR_Ambiguous;
  }

  // No need to check for deleted functions to yield OR_Deleted.

  return OR_Success;
}

/// <summary>
/// Initializes the specified <paramref name="initSequence" /> describing how
/// <paramref name="Entity" /> is initialized with <paramref name="Args" />.
/// </summary>
/// <param name="Entity">Entity being initialized; a variable, return result, etc.</param>
/// <param name="Kind">Kind of initialization: copying, list-initializing, constructing, etc.</param>
/// <param name="Args">Arguments to the initialization.</param>
/// <param name="TopLevelOfInitList">Whether this is the top-level of an initialization list.</param>
/// <param name="initSequence">Initialization sequence description to initialize.</param>
void HLSLExternalSource::InitializeInitSequenceForHLSL(
  const InitializedEntity& Entity,
  const InitializationKind& Kind,
  MultiExprArg Args,
  bool TopLevelOfInitList,
  _Inout_ InitializationSequence* initSequence)
{
  DXASSERT_NOMSG(initSequence != nullptr);

  // In HLSL there are no default initializers, eg float4x4 m();
  if (Kind.getKind() == InitializationKind::IK_Default) {
    return;
  }

  // Value initializers occur for temporaries with empty parens or braces.
  if (Kind.getKind() == InitializationKind::IK_Value) {
    m_sema->Diag(Kind.getLocation(), diag::err_hlsl_type_empty_init) << Entity.getType();
    SilenceSequenceDiagnostics(initSequence);
    return;
  }

  // If we have a DirectList, we should have a single InitListExprClass argument.
  DXASSERT(
    Kind.getKind() != InitializationKind::IK_DirectList ||
    (Args.size() == 1 && Args.front()->getStmtClass() == Stmt::InitListExprClass),
    "otherwise caller is passing in incorrect initialization configuration");

  bool isCast = Kind.isCStyleCast();
  QualType destType = Entity.getType();
  ArTypeObjectKind destShape = GetTypeObjectKind(destType);

  // Direct initialization occurs for explicit constructor arguments.
  // E.g.: http://en.cppreference.com/w/cpp/language/direct_initialization
  if (Kind.getKind() == InitializationKind::IK_Direct && destShape == AR_TOBJ_COMPOUND &&
      !Kind.isCStyleOrFunctionalCast()) {
    m_sema->Diag(Kind.getLocation(), diag::err_hlsl_require_numeric_base_for_ctor);
    SilenceSequenceDiagnostics(initSequence);
    return;
  }

  bool flatten =
    (Kind.getKind() == InitializationKind::IK_Direct && !isCast) ||
    Kind.getKind() == InitializationKind::IK_DirectList ||
    (Args.size() == 1 && Args.front()->getStmtClass() == Stmt::InitListExprClass);

  if (flatten) {
    // TODO: InitializationSequence::Perform in SemaInit should take the arity of incomplete
    // array types to adjust the value - we do calculate this as part of type analysis.
    // Until this is done, s_arr_i_f arr_struct_none[] = { }; succeeds when it should instead fail.
    FlattenedTypeIterator::ComparisonResult comparisonResult =
      FlattenedTypeIterator::CompareTypesForInit(
        *this, destType, Args,
        Kind.getLocation(), Kind.getLocation());
    if (comparisonResult.IsConvertibleAndEqualLength() ||
        (isCast && comparisonResult.IsConvertibleAndLeftLonger()))
    {
      initSequence->AddListInitializationStep(destType);
    }
    else
    {
      SourceLocation diagLocation;
      if (Args.size() > 0)
      {
        diagLocation = Args.front()->getLocStart();
      }
      else
      {
        diagLocation = Entity.getDiagLoc();
      }

      if (comparisonResult.IsEqualLength()) {
        m_sema->Diag(diagLocation, diag::err_hlsl_type_mismatch);
      }
      else {
        m_sema->Diag(diagLocation,
          diag::err_incorrect_num_initializers)
          << (comparisonResult.RightCount < comparisonResult.LeftCount)
          << IsSubobjectType(destType)
          << comparisonResult.LeftCount << comparisonResult.RightCount;
      }
      SilenceSequenceDiagnostics(initSequence);
    }
  }
  else {
    DXASSERT(Args.size() == 1, "otherwise this was mis-parsed or should be a list initialization");
    Expr* firstArg = Args.front();
    if (IsExpressionBinaryComma(firstArg)) {
      m_sema->Diag(firstArg->getExprLoc(), diag::warn_hlsl_comma_in_init);
    }

    ExprResult expr = ExprResult(firstArg);
    Sema::CheckedConversionKind cck = Kind.isExplicitCast() ?
      Sema::CheckedConversionKind::CCK_CStyleCast :
      Sema::CheckedConversionKind::CCK_ImplicitConversion;
    unsigned int msg = 0;
    CastKind castKind;
    CXXCastPath basePath;
    SourceRange range = Kind.getRange();
    ImplicitConversionSequence ics;
    ics.setStandard();
    bool castWorked = TryStaticCastForHLSL(
      expr, destType, cck, range, msg, castKind, basePath, ListInitializationFalse, SuppressWarningsFalse, SuppressErrorsTrue, &ics.Standard);
    if (castWorked) {
      if (destType.getCanonicalType() ==
              firstArg->getType().getCanonicalType() &&
          (ics.Standard).First != ICK_Lvalue_To_Rvalue) {
        initSequence->AddCAssignmentStep(destType);
      } else {
        initSequence->AddConversionSequenceStep(ics, destType.getNonReferenceType(), TopLevelOfInitList);
      }
    }
    else {
      initSequence->SetFailed(InitializationSequence::FK_ConversionFailed);
    }
  }
}

bool HLSLExternalSource::IsConversionToLessOrEqualElements(
  const QualType& sourceType,
  const QualType& targetType,
  bool explicitConversion)
{
  DXASSERT_NOMSG(!sourceType.isNull());
  DXASSERT_NOMSG(!targetType.isNull());

  ArTypeInfo sourceTypeInfo;
  ArTypeInfo targetTypeInfo;
  GetConversionForm(sourceType, explicitConversion, &sourceTypeInfo);
  GetConversionForm(targetType, explicitConversion, &targetTypeInfo);
  if (sourceTypeInfo.EltKind != targetTypeInfo.EltKind)
  {
    return false;
  }

  bool isVecMatTrunc = sourceTypeInfo.ShapeKind == AR_TOBJ_VECTOR &&
                       targetTypeInfo.ShapeKind == AR_TOBJ_BASIC;

  if (sourceTypeInfo.ShapeKind != targetTypeInfo.ShapeKind &&
      !isVecMatTrunc)
  {
    return false;
  }

  if (sourceTypeInfo.ShapeKind == AR_TOBJ_OBJECT &&
      sourceTypeInfo.ObjKind == targetTypeInfo.ObjKind) {
    return true;
  }

  // Same struct is eqaul.
  if (sourceTypeInfo.ShapeKind == AR_TOBJ_COMPOUND &&
      sourceType.getCanonicalType().getUnqualifiedType() ==
          targetType.getCanonicalType().getUnqualifiedType()) {
    return true;
  }
  // DerivedFrom is less.
  if (sourceTypeInfo.ShapeKind == AR_TOBJ_COMPOUND ||
      GetTypeObjectKind(sourceType) == AR_TOBJ_COMPOUND) {
    const RecordType *targetRT = targetType->getAsStructureType();
    if (!targetRT)
      targetRT = dyn_cast<RecordType>(targetType);

    const RecordType *sourceRT = sourceType->getAsStructureType();
    if (!sourceRT)
      sourceRT = dyn_cast<RecordType>(sourceType);

    if (targetRT && sourceRT) {
      RecordDecl *targetRD = targetRT->getDecl();
      RecordDecl *sourceRD = sourceRT->getDecl();
      const CXXRecordDecl *targetCXXRD = dyn_cast<CXXRecordDecl>(targetRD);
      const CXXRecordDecl *sourceCXXRD = dyn_cast<CXXRecordDecl>(sourceRD);
      if (targetCXXRD && sourceCXXRD) {
        if (sourceCXXRD->isDerivedFrom(targetCXXRD))
          return true;
      }
    }
  }

  if (sourceTypeInfo.ShapeKind != AR_TOBJ_SCALAR &&
    sourceTypeInfo.ShapeKind != AR_TOBJ_VECTOR &&
    sourceTypeInfo.ShapeKind != AR_TOBJ_MATRIX)
  {
    return false;
  }

  return targetTypeInfo.uTotalElts <= sourceTypeInfo.uTotalElts;
}

bool HLSLExternalSource::IsConversionToLessOrEqualElements(
  const ExprResult& sourceExpr,
  const QualType& targetType,
  bool explicitConversion)
{
  if (sourceExpr.isInvalid() || targetType.isNull())
  {
    return false;
  }

  return IsConversionToLessOrEqualElements(sourceExpr.get()->getType(), targetType, explicitConversion);
}

bool HLSLExternalSource::IsTypeNumeric(QualType type, UINT* count)
{
  DXASSERT_NOMSG(!type.isNull());
  DXASSERT_NOMSG(count != nullptr);

  *count = 0;
  UINT subCount = 0;
  ArTypeObjectKind shapeKind = GetTypeObjectKind(type);
  switch (shapeKind)
  {
  case AR_TOBJ_ARRAY:
    if (IsTypeNumeric(m_context->getAsArrayType(type)->getElementType(), &subCount))
    {
      *count = subCount * GetArraySize(type);
      return true;
    }
    return false;
  case AR_TOBJ_COMPOUND:
    {
      UINT maxCount = 0;
      { // Determine maximum count to prevent infinite loop on incomplete array
        FlattenedTypeIterator itCount(SourceLocation(), type, *this);
        maxCount = itCount.countRemaining();
        if (!maxCount) {
          return false; // empty struct.
        }
      }
      FlattenedTypeIterator it(SourceLocation(), type, *this);
      while (it.hasCurrentElement()) {
        bool isFieldNumeric = IsTypeNumeric(it.getCurrentElement(), &subCount);
        if (!isFieldNumeric) {
          return false;
        }
        if (*count >= maxCount) {
          // this element is an incomplete array at the end; iterator will not advance past this element.
          // don't add to *count either, so *count will represent minimum size of the structure.
          break;
        }
        *count += (subCount * it.getCurrentElementSize());
        it.advanceCurrentElement(it.getCurrentElementSize());
      }
      return true;
    }
  default:
    DXASSERT(false, "unreachable");
  case AR_TOBJ_BASIC:
  case AR_TOBJ_MATRIX:
  case AR_TOBJ_VECTOR:
    *count = GetElementCount(type);
    return IsBasicKindNumeric(GetTypeElementKind(type));
  case AR_TOBJ_OBJECT:
  case AR_TOBJ_STRING:
    return false;
  }
}

enum MatrixMemberAccessError {
  MatrixMemberAccessError_None,               // No errors found.
  MatrixMemberAccessError_BadFormat,          // Formatting error (non-digit).
  MatrixMemberAccessError_MixingRefs,         // Mix of zero-based and one-based references.
  MatrixMemberAccessError_Empty,              // No members specified.
  MatrixMemberAccessError_ZeroInOneBased,     // A zero was used in a one-based reference.
  MatrixMemberAccessError_FourInZeroBased,    // A four was used in a zero-based reference.
  MatrixMemberAccessError_TooManyPositions,   // Too many positions (more than four) were specified.
};

static
MatrixMemberAccessError TryConsumeMatrixDigit(const char*& memberText, uint32_t* value)
{
  DXASSERT_NOMSG(memberText != nullptr);
  DXASSERT_NOMSG(value != nullptr);

  if ('0' <= *memberText && *memberText <= '9')
  {
    *value = (*memberText) - '0';
  }
  else
  {
    return MatrixMemberAccessError_BadFormat;
  }

  memberText++;
  return MatrixMemberAccessError_None;
}

static
MatrixMemberAccessError TryParseMatrixMemberAccess(_In_z_ const char* memberText, _Out_ MatrixMemberAccessPositions* value)
{
  DXASSERT_NOMSG(memberText != nullptr);
  DXASSERT_NOMSG(value != nullptr);

  MatrixMemberAccessPositions result;
  bool zeroBasedDecided = false;
  bool zeroBased = false;

  // Set the output value to invalid to allow early exits when errors are found.
  value->IsValid = 0;

  // Assume this is true until proven otherwise.
  result.IsValid = 1;
  result.Count = 0;

  while (*memberText)
  {
    // Check for a leading underscore.
    if (*memberText != '_')
    {
      return MatrixMemberAccessError_BadFormat;
    }
    ++memberText;

    // Check whether we have an 'm' or a digit.
    if (*memberText == 'm')
    {
      if (zeroBasedDecided && !zeroBased)
      {
        return MatrixMemberAccessError_MixingRefs;
      }
      zeroBased = true;
      zeroBasedDecided = true;
      ++memberText;
    }
    else if (!('0' <= *memberText && *memberText <= '9'))
    {
      return MatrixMemberAccessError_BadFormat;
    }
    else
    {
      if (zeroBasedDecided && zeroBased)
      {
        return MatrixMemberAccessError_MixingRefs;
      }
      zeroBased = false;
      zeroBasedDecided = true;
    }

    // Consume two digits for the position.
    uint32_t rowPosition;
    uint32_t colPosition;
    MatrixMemberAccessError digitError;
    if (MatrixMemberAccessError_None != (digitError = TryConsumeMatrixDigit(memberText, &rowPosition)))
    {
      return digitError;
    }
    if (MatrixMemberAccessError_None != (digitError = TryConsumeMatrixDigit(memberText, &colPosition)))
    {
      return digitError;
    }

    // Look for specific common errors (developer likely mixed up reference style).
    if (zeroBased)
    {
      if (rowPosition == 4 || colPosition == 4)
      {
        return MatrixMemberAccessError_FourInZeroBased;
      }
    }
    else
    {
      if (rowPosition == 0 || colPosition == 0)
      {
        return MatrixMemberAccessError_ZeroInOneBased;
      }

      // SetPosition will use zero-based indices.
      --rowPosition;
      --colPosition;
    }

    if (result.Count == 4)
    {
      return MatrixMemberAccessError_TooManyPositions;
    }

    result.SetPosition(result.Count, rowPosition, colPosition);
    result.Count++;
  }

  if (result.Count == 0)
  {
    return MatrixMemberAccessError_Empty;
  }

  *value = result;
  return MatrixMemberAccessError_None;
}

bool HLSLExternalSource::LookupMatrixMemberExprForHLSL(
  Expr& BaseExpr,
  DeclarationName MemberName,
  bool IsArrow,
  SourceLocation OpLoc,
  SourceLocation MemberLoc,
  ExprResult* result)
{
  DXASSERT_NOMSG(result != nullptr);

  QualType BaseType = BaseExpr.getType();
  DXASSERT(!BaseType.isNull(), "otherwise caller should have stopped analysis much earlier");

  // Assume failure.
  *result = ExprError();

  if (GetTypeObjectKind(BaseType) != AR_TOBJ_MATRIX)
  {
    return false;
  }

  QualType elementType;
  UINT rowCount, colCount;
  GetRowsAndCols(BaseType, rowCount, colCount);
  elementType = GetMatrixOrVectorElementType(BaseType);

  IdentifierInfo *member = MemberName.getAsIdentifierInfo();
  const char *memberText = member->getNameStart();
  MatrixMemberAccessPositions positions;
  MatrixMemberAccessError memberAccessError;
  unsigned msg = 0;

  memberAccessError = TryParseMatrixMemberAccess(memberText, &positions);
  switch (memberAccessError)
  {
  case MatrixMemberAccessError_BadFormat:
    msg = diag::err_hlsl_matrix_member_bad_format;
    break;
  case MatrixMemberAccessError_Empty:
    msg = diag::err_hlsl_matrix_member_empty;
    break;
  case MatrixMemberAccessError_FourInZeroBased:
    msg = diag::err_hlsl_matrix_member_four_in_zero_based;
    break;
  case MatrixMemberAccessError_MixingRefs:
    msg = diag::err_hlsl_matrix_member_mixing_refs;
    break;
  case MatrixMemberAccessError_None:
    msg = 0;
    DXASSERT(positions.IsValid, "otherwise an error should have been returned");
    // Check the position with the type now.
    for (unsigned int i = 0; i < positions.Count; i++)
    {
      uint32_t rowPos, colPos;
      positions.GetPosition(i, &rowPos, &colPos);
      if (rowPos >= rowCount || colPos >= colCount)
      {
        msg = diag::err_hlsl_matrix_member_out_of_bounds;
        break;
      }
    }
    break;
  case MatrixMemberAccessError_TooManyPositions:
    msg = diag::err_hlsl_matrix_member_too_many_positions;
    break;
  case MatrixMemberAccessError_ZeroInOneBased:
    msg = diag::err_hlsl_matrix_member_zero_in_one_based;
    break;
  default:
    llvm_unreachable("Unknown MatrixMemberAccessError value");
  }

  if (msg != 0)
  {
    m_sema->Diag(MemberLoc, msg) << memberText;

    // It's possible that it's a simple out-of-bounds condition. In this case,
    // generate the member access expression with the correct arity and continue
    // processing.
    if (!positions.IsValid)
    {
      return true;
    }
  }

  DXASSERT(positions.IsValid, "otherwise an error should have been returned");

  // Consume elements
  QualType resultType;
  if (positions.Count == 1)
    resultType = elementType;
  else
    resultType = NewSimpleAggregateType(AR_TOBJ_UNKNOWN, GetTypeElementKind(elementType), 0, OneRow, positions.Count);

  // Add qualifiers from BaseType.
  resultType = m_context->getQualifiedType(resultType, BaseType.getQualifiers());

  ExprValueKind VK =
    positions.ContainsDuplicateElements() ? VK_RValue :
      (IsArrow ? VK_LValue : BaseExpr.getValueKind());
  ExtMatrixElementExpr* matrixExpr = new (m_context)ExtMatrixElementExpr(resultType, VK, &BaseExpr, *member, MemberLoc, positions);
  *result = matrixExpr;

  return true;
}

enum VectorMemberAccessError {
  VectorMemberAccessError_None,               // No errors found.
  VectorMemberAccessError_BadFormat,          // Formatting error (not in 'rgba' or 'xyzw').
  VectorMemberAccessError_MixingStyles,       // Mix of rgba and xyzw swizzle styles.
  VectorMemberAccessError_Empty,              // No members specified.
  VectorMemberAccessError_TooManyPositions,   // Too many positions (more than four) were specified.
};

static
VectorMemberAccessError TryConsumeVectorDigit(const char*& memberText, uint32_t* value, bool &rgbaStyle) {
  DXASSERT_NOMSG(memberText != nullptr);
  DXASSERT_NOMSG(value != nullptr);

  rgbaStyle = false;

  switch (*memberText) {
  case 'r':
    rgbaStyle = true;
  case 'x':
    *value = 0;
    break;

  case 'g':
    rgbaStyle = true;
  case 'y':
    *value = 1;
    break;

  case 'b':
    rgbaStyle = true;
  case 'z':
    *value = 2;
    break;

  case 'a':
    rgbaStyle = true;
  case 'w':
    *value = 3;
    break;

  default:
    return VectorMemberAccessError_BadFormat;
  }

  memberText++;
  return VectorMemberAccessError_None;
}

static
VectorMemberAccessError TryParseVectorMemberAccess(_In_z_ const char* memberText, _Out_ VectorMemberAccessPositions* value) {
  DXASSERT_NOMSG(memberText != nullptr);
  DXASSERT_NOMSG(value != nullptr);

  VectorMemberAccessPositions result;
  bool rgbaStyleDecided = false;
  bool rgbaStyle = false;

  // Set the output value to invalid to allow early exits when errors are found.
  value->IsValid = 0;

  // Assume this is true until proven otherwise.
  result.IsValid = 1;
  result.Count = 0;

  while (*memberText) {
    // Consume one character for the swizzle.
    uint32_t colPosition;
    VectorMemberAccessError digitError;
    bool rgbaStyleTmp = false;
    if (VectorMemberAccessError_None != (digitError = TryConsumeVectorDigit(memberText, &colPosition, rgbaStyleTmp))) {
      return digitError;
    }

    if (rgbaStyleDecided && rgbaStyleTmp != rgbaStyle) {
      return VectorMemberAccessError_MixingStyles;
    }
    else {
      rgbaStyleDecided = true;
      rgbaStyle = rgbaStyleTmp;
    }

    if (result.Count == 4) {
      return VectorMemberAccessError_TooManyPositions;
    }

    result.SetPosition(result.Count, colPosition);
    result.Count++;
  }

  if (result.Count == 0) {
    return VectorMemberAccessError_Empty;
  }

  *value = result;
  return VectorMemberAccessError_None;
}

bool HLSLExternalSource::LookupVectorMemberExprForHLSL(
    Expr& BaseExpr,
    DeclarationName MemberName,
    bool IsArrow,
    SourceLocation OpLoc,
    SourceLocation MemberLoc,
    ExprResult* result) {
  DXASSERT_NOMSG(result != nullptr);

  QualType BaseType = BaseExpr.getType();
  DXASSERT(!BaseType.isNull(), "otherwise caller should have stopped analysis much earlier");

  // Assume failure.
  *result = ExprError();

  if (GetTypeObjectKind(BaseType) != AR_TOBJ_VECTOR) {
    return false;
  }

  QualType elementType;
  UINT colCount = GetHLSLVecSize(BaseType);
  elementType = GetMatrixOrVectorElementType(BaseType);

  IdentifierInfo *member = MemberName.getAsIdentifierInfo();
  const char *memberText = member->getNameStart();
  VectorMemberAccessPositions positions;
  VectorMemberAccessError memberAccessError;
  unsigned msg = 0;

  memberAccessError = TryParseVectorMemberAccess(memberText, &positions);
  switch (memberAccessError) {
  case VectorMemberAccessError_BadFormat:
    msg = diag::err_hlsl_vector_member_bad_format;
    break;
  case VectorMemberAccessError_Empty:
    msg = diag::err_hlsl_vector_member_empty;
    break;
  case VectorMemberAccessError_MixingStyles:
    msg = diag::err_ext_vector_component_name_mixedsets;
    break;
  case VectorMemberAccessError_None:
    msg = 0;
    DXASSERT(positions.IsValid, "otherwise an error should have been returned");
    // Check the position with the type now.
    for (unsigned int i = 0; i < positions.Count; i++) {
      uint32_t colPos;
      positions.GetPosition(i, &colPos);
      if (colPos >= colCount) {
        msg = diag::err_hlsl_vector_member_out_of_bounds;
        break;
      }
    }
    break;
  case VectorMemberAccessError_TooManyPositions:
    msg = diag::err_hlsl_vector_member_too_many_positions;
    break;
  default:
    llvm_unreachable("Unknown VectorMemberAccessError value");
  }

  if (msg != 0) {
    m_sema->Diag(MemberLoc, msg) << memberText;

    // It's possible that it's a simple out-of-bounds condition. In this case,
    // generate the member access expression with the correct arity and continue
    // processing.
    if (!positions.IsValid) {
      return true;
    }
  }

  DXASSERT(positions.IsValid, "otherwise an error should have been returned");

  // Consume elements
  QualType resultType;
  if (positions.Count == 1)
    resultType = elementType;
  else
    resultType = NewSimpleAggregateType(AR_TOBJ_UNKNOWN, GetTypeElementKind(elementType), 0, OneRow, positions.Count);
  
  // Add qualifiers from BaseType.
  resultType = m_context->getQualifiedType(resultType, BaseType.getQualifiers());

  ExprValueKind VK =
    positions.ContainsDuplicateElements() ? VK_RValue :
      (IsArrow ? VK_LValue : BaseExpr.getValueKind());
  HLSLVectorElementExpr* vectorExpr = new (m_context)HLSLVectorElementExpr(resultType, VK, &BaseExpr, *member, MemberLoc, positions);
  *result = vectorExpr;

  return true;
}

bool HLSLExternalSource::LookupArrayMemberExprForHLSL(
  Expr& BaseExpr,
  DeclarationName MemberName,
  bool IsArrow,
  SourceLocation OpLoc,
  SourceLocation MemberLoc,
  ExprResult* result) {

  DXASSERT_NOMSG(result != nullptr);

  QualType BaseType = BaseExpr.getType();
  DXASSERT(!BaseType.isNull(), "otherwise caller should have stopped analysis much earlier");

  // Assume failure.
  *result = ExprError();

  if (GetTypeObjectKind(BaseType) != AR_TOBJ_ARRAY) {
    return false;
  }

  IdentifierInfo *member = MemberName.getAsIdentifierInfo();
  const char *memberText = member->getNameStart();

  // The only property available on arrays is Length; it is deprecated and available only on HLSL version <=2018
  if (member->getLength() == 6 && 0 == strcmp(memberText, "Length")) {
    if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(BaseType)) {
      // check version support
      unsigned hlslVer = getSema()->getLangOpts().HLSLVersion;
      if (hlslVer > 2016) {
        m_sema->Diag(MemberLoc, diag::err_hlsl_unsupported_for_version_lower) << "Length" << "2016";
        return false;
      }
      if (hlslVer == 2016) {
        m_sema->Diag(MemberLoc, diag::warn_deprecated) << "Length";
      }

      UnaryExprOrTypeTraitExpr *arrayLenExpr = new (m_context) UnaryExprOrTypeTraitExpr(
        UETT_ArrayLength, &BaseExpr, m_context->getSizeType(), MemberLoc, BaseExpr.getSourceRange().getEnd());

      *result = arrayLenExpr;
      return true;
    }
  }
  return false;
}
  

ExprResult HLSLExternalSource::MaybeConvertScalarToVector(_In_ clang::Expr* E) {
  DXASSERT_NOMSG(E != nullptr);
  ArBasicKind basic = GetTypeElementKind(E->getType());
  if (!IS_BASIC_PRIMITIVE(basic)) {
    return E;
  }

  ArTypeObjectKind kind = GetTypeObjectKind(E->getType());
  if (kind != AR_TOBJ_SCALAR) {
    return E;
  }

  QualType targetType = NewSimpleAggregateType(AR_TOBJ_VECTOR, basic, 0, 1, 1);
  return ImplicitCastExpr::Create(*m_context, targetType, CastKind::CK_HLSLVectorSplat, E, nullptr, E->getValueKind());
}

static clang::CastKind ImplicitConversionKindToCastKind(
    clang::ImplicitConversionKind ICK, 
    ArBasicKind FromKind, 
    ArBasicKind ToKind) {
  // TODO: Shouldn't we have more specific ICK enums so we don't have to re-evaluate
  //        based on from/to kinds in order to determine CastKind?
  //  There's a FIXME note in PerformImplicitConversion that calls out exactly this
  //  problem.
  switch (ICK) {
  case ICK_Integral_Promotion:
  case ICK_Integral_Conversion:
    return CK_IntegralCast;
  case ICK_Floating_Promotion:
  case ICK_Floating_Conversion:
    return CK_FloatingCast;
  case ICK_Floating_Integral:
    if (IS_BASIC_FLOAT(FromKind) && IS_BASIC_AINT(ToKind))
      return CK_FloatingToIntegral;
    else if ((IS_BASIC_AINT(FromKind) || IS_BASIC_BOOL(FromKind)) && IS_BASIC_FLOAT(ToKind))
      return CK_IntegralToFloating;
    break;
  case ICK_Boolean_Conversion:
    if (IS_BASIC_FLOAT(FromKind) && IS_BASIC_BOOL(ToKind))
      return CK_FloatingToBoolean;
    else if (IS_BASIC_AINT(FromKind) && IS_BASIC_BOOL(ToKind))
      return CK_IntegralToBoolean;
    break;
  default:
    // Only covers implicit conversions with cast kind equivalents.
    return CK_Invalid;
  }
  return CK_Invalid;
}
static clang::CastKind ConvertToComponentCastKind(clang::CastKind CK) {
  switch (CK) {
  case CK_IntegralCast:
    return CK_HLSLCC_IntegralCast;
  case CK_FloatingCast:
    return CK_HLSLCC_FloatingCast;
  case CK_FloatingToIntegral:
    return CK_HLSLCC_FloatingToIntegral;
  case CK_IntegralToFloating:
    return CK_HLSLCC_IntegralToFloating;
  case CK_FloatingToBoolean:
    return CK_HLSLCC_FloatingToBoolean;
  case CK_IntegralToBoolean:
    return CK_HLSLCC_IntegralToBoolean;
  default:
    // Only HLSLCC castkinds are relevant. Ignore the rest.
    return CK_Invalid;
  }
  return CK_Invalid;
}

clang::Expr *HLSLExternalSource::HLSLImpCastToScalar(
  _In_ clang::Sema* self,
  _In_ clang::Expr* From,
  ArTypeObjectKind FromShape,
  ArBasicKind EltKind)
{
  clang::CastKind CK = CK_Invalid;
  if (AR_TOBJ_MATRIX == FromShape)
    CK = CK_HLSLMatrixToScalarCast;
  if (AR_TOBJ_VECTOR == FromShape)
    CK = CK_HLSLVectorToScalarCast;
  if (CK_Invalid != CK) {
    return self->ImpCastExprToType(From, 
      NewSimpleAggregateType(AR_TOBJ_BASIC, EltKind, 0, 1, 1), CK, From->getValueKind()).get();
  }
  return From;
}

clang::ExprResult HLSLExternalSource::PerformHLSLConversion(
  _In_ clang::Expr* From,
  _In_ clang::QualType targetType,
  _In_ const clang::StandardConversionSequence &SCS,
  _In_ clang::Sema::CheckedConversionKind CCK)
{
  QualType sourceType = From->getType();
  sourceType = GetStructuralForm(sourceType);
  targetType = GetStructuralForm(targetType);
  ArTypeInfo SourceInfo, TargetInfo;
  CollectInfo(sourceType, &SourceInfo);
  CollectInfo(targetType, &TargetInfo);

  clang::CastKind CK = CK_Invalid;
  QualType intermediateTarget;

  // TODO: construct vector/matrix and component cast expressions
  switch (SCS.Second) {
    case ICK_Flat_Conversion: {
      // TODO: determine how to handle individual component conversions:
      // - have an array of conversions for ComponentConversion in SCS?
      //    convert that to an array of casts under a special kind of flat
      //    flat conversion node?  What do component conversion casts cast
      //    from?  We don't have a From expression for individiual components.
      From = m_sema->ImpCastExprToType(From, targetType.getUnqualifiedType(), CK_FlatConversion, From->getValueKind(), /*BasePath=*/0, CCK).get();
      break;
    }
    case ICK_HLSL_Derived_To_Base: {
      CXXCastPath BasePath;
      if (m_sema->CheckDerivedToBaseConversion(
              sourceType, targetType.getNonReferenceType(), From->getLocStart(),
              From->getSourceRange(), &BasePath, /*IgnoreAccess=*/true))
        return ExprError();

      From = m_sema->ImpCastExprToType(From, targetType.getUnqualifiedType(), CK_HLSLDerivedToBase, From->getValueKind(), &BasePath, CCK).get();
      break;
    }
    case ICK_HLSLVector_Splat: {
      // 1. optionally convert from vec1 or mat1x1 to scalar
      From = HLSLImpCastToScalar(m_sema, From, SourceInfo.ShapeKind, SourceInfo.EltKind);
      // 2. optionally convert component type
      if (ICK_Identity != SCS.ComponentConversion) {
        CK = ImplicitConversionKindToCastKind(SCS.ComponentConversion, SourceInfo.EltKind, TargetInfo.EltKind);
        if (CK_Invalid != CK) {
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_BASIC, TargetInfo.EltKind, 0, 1, 1), CK, From->getValueKind(), /*BasePath=*/0, CCK).get();
        }
      }
      // 3. splat scalar to final vector or matrix
      CK = CK_Invalid;
      if (AR_TOBJ_VECTOR == TargetInfo.ShapeKind)
        CK = CK_HLSLVectorSplat;
      else if (AR_TOBJ_MATRIX == TargetInfo.ShapeKind)
        CK = CK_HLSLMatrixSplat;
      if (CK_Invalid != CK) {
        From = m_sema->ImpCastExprToType(From, 
          NewSimpleAggregateType(TargetInfo.ShapeKind, TargetInfo.EltKind, 0, TargetInfo.uRows, TargetInfo.uCols), CK, From->getValueKind(), /*BasePath=*/0, CCK).get();
      }
      break;
    }
    case ICK_HLSLVector_Scalar: {
      // 1. select vector or matrix component
      From = HLSLImpCastToScalar(m_sema, From, SourceInfo.ShapeKind, SourceInfo.EltKind);
      // 2. optionally convert component type
      if (ICK_Identity != SCS.ComponentConversion) {
        CK = ImplicitConversionKindToCastKind(SCS.ComponentConversion, SourceInfo.EltKind, TargetInfo.EltKind);
        if (CK_Invalid != CK) {
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_BASIC, TargetInfo.EltKind, 0, 1, 1), CK, From->getValueKind(), /*BasePath=*/0, CCK).get();
        }
      }
      break;
    }

    // The following two (three if we re-introduce ICK_HLSLComponent_Conversion) steps
    // can be done with case fall-through, since this is the order in which we want to 
    // do the conversion operations.
    case ICK_HLSLVector_Truncation: {
      // 1. dimension truncation
      // vector truncation or matrix truncation?
      if (SourceInfo.ShapeKind == AR_TOBJ_VECTOR) {
        From = m_sema->ImpCastExprToType(From, 
          NewSimpleAggregateType(AR_TOBJ_VECTOR, SourceInfo.EltKind, 0, 1, TargetInfo.uTotalElts), 
          CK_HLSLVectorTruncationCast, From->getValueKind(), /*BasePath=*/0, CCK).get();
      } else if (SourceInfo.ShapeKind == AR_TOBJ_MATRIX) {
        if (TargetInfo.ShapeKind == AR_TOBJ_VECTOR && 1 == SourceInfo.uCols) {
          // Handle the column to vector case
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_MATRIX, SourceInfo.EltKind, 0, TargetInfo.uCols, 1), 
            CK_HLSLMatrixTruncationCast, From->getValueKind(), /*BasePath=*/0, CCK).get();
        } else {
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_MATRIX, SourceInfo.EltKind, 0, TargetInfo.uRows, TargetInfo.uCols), 
            CK_HLSLMatrixTruncationCast, From->getValueKind(), /*BasePath=*/0, CCK).get();
        }
      } else {
        DXASSERT(false, "PerformHLSLConversion: Invalid source type for truncation cast");
      }
    }
    __fallthrough;

    case ICK_HLSLVector_Conversion: {
      // 2. Do ShapeKind conversion if necessary
      if (SourceInfo.ShapeKind != TargetInfo.ShapeKind) {
        switch (TargetInfo.ShapeKind) {
        case AR_TOBJ_VECTOR:
          DXASSERT(AR_TOBJ_MATRIX == SourceInfo.ShapeKind, "otherwise, invalid casting sequence");
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_VECTOR, SourceInfo.EltKind, 0, TargetInfo.uRows, TargetInfo.uCols), 
            CK_HLSLMatrixToVectorCast, From->getValueKind(), /*BasePath=*/0, CCK).get();
          break;
        case AR_TOBJ_MATRIX:
          DXASSERT(AR_TOBJ_VECTOR == SourceInfo.ShapeKind, "otherwise, invalid casting sequence");
          From = m_sema->ImpCastExprToType(From, 
            NewSimpleAggregateType(AR_TOBJ_MATRIX, SourceInfo.EltKind, 0, TargetInfo.uRows, TargetInfo.uCols), 
            CK_HLSLVectorToMatrixCast, From->getValueKind(), /*BasePath=*/0, CCK).get();
          break;
        case AR_TOBJ_BASIC:
          // Truncation may be followed by cast to scalar
          From = HLSLImpCastToScalar(m_sema, From, SourceInfo.ShapeKind, SourceInfo.EltKind);
          break;
        default:
          DXASSERT(false, "otherwise, invalid casting sequence");
          break;
        }
      }

      // 3. Do component type conversion
      if (ICK_Identity != SCS.ComponentConversion) {
        CK = ImplicitConversionKindToCastKind(SCS.ComponentConversion, SourceInfo.EltKind, TargetInfo.EltKind);
        if (TargetInfo.ShapeKind != AR_TOBJ_BASIC)
          CK = ConvertToComponentCastKind(CK);
        if (CK_Invalid != CK) {
          From = m_sema->ImpCastExprToType(From, targetType, CK, From->getValueKind(), /*BasePath=*/0, CCK).get();
        }
      }
      break;
    }
    case ICK_Identity:
      // Nothing to do.
      break;
    default:
      DXASSERT(false, "PerformHLSLConversion: Invalid SCS.Second conversion kind");
  }
  return From;
}

void HLSLExternalSource::GetConversionForm(
  QualType type,
  bool explicitConversion,
  ArTypeInfo* pTypeInfo)
{
  //if (!CollectInfo(AR_TINFO_ALLOW_ALL, pTypeInfo))
  CollectInfo(type, pTypeInfo);

  // The fxc implementation reported pTypeInfo->ShapeKind separately in an output argument,
  // but that value is only used for pointer conversions.

  // When explicitly converting types complex aggregates can be treated
  // as vectors if they are entirely numeric.
  switch (pTypeInfo->ShapeKind)
  {
  case AR_TOBJ_COMPOUND:
  case AR_TOBJ_ARRAY:
    if (explicitConversion && IsTypeNumeric(type, &pTypeInfo->uTotalElts))
    {
      pTypeInfo->ShapeKind = AR_TOBJ_VECTOR;
    }
    else
    {
      pTypeInfo->ShapeKind = AR_TOBJ_COMPOUND;
    }

    DXASSERT_NOMSG(pTypeInfo->uRows == 1);
    pTypeInfo->uCols = pTypeInfo->uTotalElts;
    break;

  case AR_TOBJ_VECTOR:
  case AR_TOBJ_MATRIX:
    // Convert 1x1 types to scalars.
    if (pTypeInfo->uCols == 1 && pTypeInfo->uRows == 1)
    {
      pTypeInfo->ShapeKind = AR_TOBJ_BASIC;
    }
    break;
  default:
    // Only convertable shapekinds are relevant.
    break;
  }
}

static
bool HandleVoidConversion(QualType source, QualType target, bool explicitConversion, _Out_ bool* allowed)
{
  DXASSERT_NOMSG(allowed != nullptr);
  bool applicable = true;
  *allowed = true;
  if (explicitConversion) {
    // (void) non-void
    if (target->isVoidType()) {
      DXASSERT_NOMSG(*allowed);
    }
    // (non-void) void
    else if (source->isVoidType()) {
      *allowed = false;
    }
    else {
      applicable = false;
    }
  }
  else {
    // (void) void
    if (source->isVoidType() && target->isVoidType()) {
      DXASSERT_NOMSG(*allowed);
    }
    // (void) non-void, (non-void) void
    else if (source->isVoidType() || target->isVoidType()) {
      *allowed = false;
    }
    else {
      applicable = false;
    }
  }
  return applicable;
}

static bool ConvertDimensions(ArTypeInfo TargetInfo, ArTypeInfo SourceInfo,
                              ImplicitConversionKind &Second,
                              TYPE_CONVERSION_REMARKS &Remarks) {
  // The rules for aggregate conversions are:
  // 1. A scalar can be replicated to any layout.
  // 2. Any type may be truncated to anything else with one component.
  // 3. A vector may be truncated to a smaller vector.
  // 4. A matrix may be truncated to a smaller matrix.
  // 5. The result of a vector and a matrix is:
  //    a. If the matrix has one row it's a vector-sized
  //       piece of the row.
  //    b. If the matrix has one column it's a vector-sized
  //       piece of the column.
  //    c. Otherwise the number of elements in the vector
  //       and matrix must match and the result is the vector.
  // 6. The result of a matrix and a vector is similar to #5.

  switch (TargetInfo.ShapeKind) {
  case AR_TOBJ_BASIC:
    switch (SourceInfo.ShapeKind)
    {
    case AR_TOBJ_BASIC:
      Second = ICK_Identity;
      break;

    case AR_TOBJ_VECTOR:
      if (1 < SourceInfo.uCols)
        Second = ICK_HLSLVector_Truncation;
      else
        Second = ICK_HLSLVector_Scalar;
      break;

    case AR_TOBJ_MATRIX:
      if (1 < SourceInfo.uRows * SourceInfo.uCols)
        Second = ICK_HLSLVector_Truncation;
      else
        Second = ICK_HLSLVector_Scalar;
      break;

    default:
      return false;
    }

    break;

  case AR_TOBJ_VECTOR:
    switch (SourceInfo.ShapeKind)
    {
    case AR_TOBJ_BASIC:
      // Conversions between scalars and aggregates are always supported.
      Second = ICK_HLSLVector_Splat;
      break;

    case AR_TOBJ_VECTOR:
      if (TargetInfo.uCols > SourceInfo.uCols) {
        if (SourceInfo.uCols == 1) {
          Second = ICK_HLSLVector_Splat;
        }
        else {
          return false;
        }
      }
      else if (TargetInfo.uCols < SourceInfo.uCols) {
        Second = ICK_HLSLVector_Truncation;
      }
      else {
        Second = ICK_Identity;
      }
      break;

    case AR_TOBJ_MATRIX: {
      UINT SourceComponents = SourceInfo.uRows * SourceInfo.uCols;
      if (1 == SourceComponents && TargetInfo.uCols != 1) {
        // splat: matrix<[..], 1, 1> -> vector<[..], O>
        Second = ICK_HLSLVector_Splat;
      }
      else if (1 == SourceInfo.uRows || 1 == SourceInfo.uCols) {
        // cases for: matrix<[..], M, N> -> vector<[..], O>, where N == 1 or M == 1
        if (TargetInfo.uCols > SourceComponents)          // illegal: O > N*M
          return false;
        else if (TargetInfo.uCols < SourceComponents)     // truncation: O < N*M
          Second = ICK_HLSLVector_Truncation;
        else                                              // equalivalent: O == N*M
          Second = ICK_HLSLVector_Conversion;
      }
      else if (TargetInfo.uCols == 1 && SourceComponents > 1) {
        Second = ICK_HLSLVector_Truncation;
      }
      else if (TargetInfo.uCols != SourceComponents) {
        // illegal: matrix<[..], M, N> -> vector<[..], O> where N != 1 and M != 1 and O != N*M
        return false;
      }
      else {
        // legal: matrix<[..], M, N> -> vector<[..], O> where N != 1 and M != 1 and O == N*M
        Second = ICK_HLSLVector_Conversion;
      }
      break;
    }

    default:
      return false;
    }

    break;

  case AR_TOBJ_MATRIX: {
    UINT TargetComponents = TargetInfo.uRows * TargetInfo.uCols;
    switch (SourceInfo.ShapeKind)
    {
    case AR_TOBJ_BASIC:
      // Conversions between scalars and aggregates are always supported.
      Second = ICK_HLSLVector_Splat;
      break;

    case AR_TOBJ_VECTOR: {
      // We can only convert vector to matrix in following cases:
      //  - splat from vector<...,1>
      //  - same number of components
      //  - one target component (truncate to scalar)
      //  - matrix has one row or one column, and fewer components (truncation)
      // Other cases disallowed even if implicitly convertable in two steps (truncation+conversion).
      if (1 == SourceInfo.uCols && TargetComponents != 1) {
        // splat: vector<[..], 1> -> matrix<[..], M, N>
        Second = ICK_HLSLVector_Splat;
      }
      else if (TargetComponents == SourceInfo.uCols) {
        // legal: vector<[..], O> -> matrix<[..], M, N> where N != 1 and M != 1 and O == N*M
        Second = ICK_HLSLVector_Conversion;
      }
      else if (1 == TargetComponents) {
        // truncate to scalar: matrix<[..], 1, 1>
        Second = ICK_HLSLVector_Truncation;
      }
      else if ((1 == TargetInfo.uRows || 1 == TargetInfo.uCols) &&
               TargetComponents < SourceInfo.uCols) {
        Second = ICK_HLSLVector_Truncation;
      }
      else {
        // illegal: change in components without going to or from scalar equivalent
        return false;
      }
      break;
    }

    case AR_TOBJ_MATRIX: {
      UINT SourceComponents = SourceInfo.uRows * SourceInfo.uCols;
      if (1 == SourceComponents && TargetComponents != 1) {
        // splat: matrix<[..], 1, 1> -> matrix<[..], M, N>
        Second = ICK_HLSLVector_Splat;
      }
      else if (TargetComponents == 1) {
        Second = ICK_HLSLVector_Truncation;
      }
      else if (TargetInfo.uRows > SourceInfo.uRows || TargetInfo.uCols > SourceInfo.uCols) {
        return false;
      }
      else if (TargetInfo.uRows < SourceInfo.uRows || TargetInfo.uCols < SourceInfo.uCols) {
        Second = ICK_HLSLVector_Truncation;
      }
      else {
        Second = ICK_Identity;
      }
      break;
    }

    default:
      return false;
    }

    break;
  }
  case AR_TOBJ_STRING:
    if (SourceInfo.ShapeKind == AR_TOBJ_STRING) {
      Second = ICK_Identity;
      break;
    }
    else {
      return false;
    }

  default:
    return false;
  }

  if (TargetInfo.uTotalElts < SourceInfo.uTotalElts)
  {
    Remarks |= TYPE_CONVERSION_ELT_TRUNCATION;
  }

  return true;
}

static bool ConvertComponent(ArTypeInfo TargetInfo, ArTypeInfo SourceInfo,
                             ImplicitConversionKind &ComponentConversion,
                             TYPE_CONVERSION_REMARKS &Remarks) {
  // Conversion to/from unknown types not supported.
  if (TargetInfo.EltKind == AR_BASIC_UNKNOWN ||
      SourceInfo.EltKind == AR_BASIC_UNKNOWN) {
    return false;
  }

  bool precisionLoss = false;
  if (GET_BASIC_BITS(TargetInfo.EltKind) != 0 &&
    GET_BASIC_BITS(TargetInfo.EltKind) <
    GET_BASIC_BITS(SourceInfo.EltKind))
  {
    precisionLoss = true;
    Remarks |= TYPE_CONVERSION_PRECISION_LOSS;
  }

  // enum -> enum not allowed
  if ((SourceInfo.EltKind == AR_BASIC_ENUM &&
    TargetInfo.EltKind == AR_BASIC_ENUM) ||
    SourceInfo.EltKind == AR_BASIC_ENUM_CLASS ||
    TargetInfo.EltKind == AR_BASIC_ENUM_CLASS) {
    return false;
  }
  if (SourceInfo.EltKind != TargetInfo.EltKind)
  {
    if (IS_BASIC_BOOL(TargetInfo.EltKind))
    {
      ComponentConversion = ICK_Boolean_Conversion;
    }
    else if (IS_BASIC_ENUM(TargetInfo.EltKind))
    {
      // conversion to enum type not allowed
      return false;
    }
    else if (IS_BASIC_ENUM(SourceInfo.EltKind))
    {
      // enum -> int/float
      ComponentConversion = ICK_Integral_Conversion;
    }
    else if (TargetInfo.EltKind == AR_OBJECT_STRING) 
    {
      if (SourceInfo.EltKind == AR_OBJECT_STRING_LITERAL) {
        ComponentConversion = ICK_Array_To_Pointer;
      }
      else 
      {
        return false;
      }
    }
    else
    {
      bool targetIsInt = IS_BASIC_AINT(TargetInfo.EltKind);
      if (IS_BASIC_AINT(SourceInfo.EltKind))
      {
        if (targetIsInt)
        {
          ComponentConversion = precisionLoss ? ICK_Integral_Conversion : ICK_Integral_Promotion;
        }
        else
        {
          ComponentConversion = ICK_Floating_Integral;
        }
      }
      else if (IS_BASIC_FLOAT(SourceInfo.EltKind))
      {
        if (targetIsInt)
        {
          ComponentConversion = ICK_Floating_Integral;
        }
        else
        {
          ComponentConversion = precisionLoss ? ICK_Floating_Conversion : ICK_Floating_Promotion;
        }
      }
      else if (IS_BASIC_BOOL(SourceInfo.EltKind)) {
        if (targetIsInt)
          ComponentConversion = ICK_Integral_Conversion;
        else
          ComponentConversion = ICK_Floating_Integral;
      }
    }
  }

  return true;
}

_Use_decl_annotations_
bool HLSLExternalSource::CanConvert(
  SourceLocation loc,
  Expr* sourceExpr,
  QualType target,
  bool explicitConversion,
  _Out_opt_ TYPE_CONVERSION_REMARKS* remarks,
  _Inout_opt_ StandardConversionSequence* standard)
{
  UINT uTSize, uSSize;
  bool SourceIsAggregate, TargetIsAggregate; // Early declarations due to gotos below

  DXASSERT_NOMSG(sourceExpr != nullptr);
  DXASSERT_NOMSG(!target.isNull());

  // Implements the semantics of ArType::CanConvertTo.
  TYPE_CONVERSION_FLAGS Flags = explicitConversion ? TYPE_CONVERSION_EXPLICIT : TYPE_CONVERSION_DEFAULT;
  TYPE_CONVERSION_REMARKS Remarks = TYPE_CONVERSION_NONE;
  QualType source = sourceExpr->getType();
  // Cannot cast function type.
  if (source->isFunctionType())
    return false;

  // Convert to an r-value to begin with, with an exception for strings
  // since they are not first-class values and we want to preserve them as literals.
  bool needsLValueToRValue = sourceExpr->isLValue() && !target->isLValueReferenceType()
    && sourceExpr->getStmtClass() != Expr::StringLiteralClass;

  bool targetRef = target->isReferenceType();

  // Initialize the output standard sequence if available.
  if (standard != nullptr) {
    // Set up a no-op conversion, other than lvalue to rvalue - HLSL does not support references.
    standard->setAsIdentityConversion();
    if (needsLValueToRValue) {
      standard->First = ICK_Lvalue_To_Rvalue;
    }

    standard->setFromType(source);
    standard->setAllToTypes(target);
  }

  source = GetStructuralForm(source);
  target = GetStructuralForm(target);

  // Temporary conversion kind tracking which will be used/fixed up at the end
  ImplicitConversionKind Second = ICK_Identity;
  ImplicitConversionKind ComponentConversion = ICK_Identity;

  // Identical types require no conversion.
  if (source == target) {
    Remarks = TYPE_CONVERSION_IDENTICAL;
    goto lSuccess;
  }

  // Trivial cases for void.
  bool allowed;
  if (HandleVoidConversion(source, target, explicitConversion, &allowed)) {
    if (allowed) {
      Remarks = target->isVoidType() ? TYPE_CONVERSION_TO_VOID : Remarks;
      goto lSuccess;
    }
    else {
      return false;
    }
  }

  ArTypeInfo TargetInfo, SourceInfo;
  CollectInfo(target, &TargetInfo);
  CollectInfo(source, &SourceInfo);

  uTSize = TargetInfo.uTotalElts;
  uSSize = SourceInfo.uTotalElts;

  // TODO: TYPE_CONVERSION_BY_REFERENCE does not seem possible here
  // are we missing cases?
  if ((Flags & TYPE_CONVERSION_BY_REFERENCE) != 0 && uTSize != uSSize) {
    return false;
  }

  // Structure cast.
  SourceIsAggregate = SourceInfo.ShapeKind == AR_TOBJ_COMPOUND || SourceInfo.ShapeKind == AR_TOBJ_ARRAY;
  TargetIsAggregate = TargetInfo.ShapeKind == AR_TOBJ_COMPOUND || TargetInfo.ShapeKind == AR_TOBJ_ARRAY;
  if (SourceIsAggregate || TargetIsAggregate) {
    // For implicit conversions, FXC treats arrays the same as structures
    // and rejects conversions between them and numeric types
    if (!explicitConversion && SourceIsAggregate != TargetIsAggregate)
    {
      return false;
    }

    // Structure to structure cases
    const RecordType *targetRT = dyn_cast<RecordType>(target);
    const RecordType *sourceRT = dyn_cast<RecordType>(source);
    if (targetRT && sourceRT) {
      RecordDecl *targetRD = targetRT->getDecl();
      RecordDecl *sourceRD = sourceRT->getDecl();
      if (sourceRT && targetRT) {
        if (targetRD == sourceRD) {
          Second = ICK_Flat_Conversion;
          goto lSuccess;
        }

        const CXXRecordDecl* targetCXXRD = dyn_cast<CXXRecordDecl>(targetRD);
        const CXXRecordDecl* sourceCXXRD = dyn_cast<CXXRecordDecl>(sourceRD);
        if (targetCXXRD && sourceCXXRD && sourceCXXRD->isDerivedFrom(targetCXXRD)) {
          Second = ICK_HLSL_Derived_To_Base;
          goto lSuccess;
        }
      }
    }

    // Handle explicit splats from single element numerical types (scalars, vector1s and matrix1x1s) to aggregate types.
    if (explicitConversion) {
      const BuiltinType *sourceSingleElementBuiltinType = source->getAs<BuiltinType>();
      if (sourceSingleElementBuiltinType == nullptr
        && hlsl::IsHLSLVecMatType(source)
        && hlsl::GetElementCount(source) == 1) {
        sourceSingleElementBuiltinType = hlsl::GetElementTypeOrType(source)->getAs<BuiltinType>();
      }

      // We can only splat to target types that do not contain object/resource types
      if (sourceSingleElementBuiltinType != nullptr && hlsl::IsHLSLNumericOrAggregateOfNumericType(target)) {
        BuiltinType::Kind kind = sourceSingleElementBuiltinType->getKind();
        switch (kind) {
        case BuiltinType::Kind::UInt:
        case BuiltinType::Kind::Int:
        case BuiltinType::Kind::Float:
        case BuiltinType::Kind::LitFloat:
        case BuiltinType::Kind::LitInt:
          Second = ICK_Flat_Conversion;
          goto lSuccess;
        default:
          // Only flat conversion kinds are relevant.
          break;
        }
      }
    }

    FlattenedTypeIterator::ComparisonResult result =
      FlattenedTypeIterator::CompareTypes(*this, loc, loc, target, source);
    if (!result.CanConvertElements) {
      return false;
    }

    // Only allow scalar to compound or array with explicit cast
    if (result.IsConvertibleAndLeftLonger()) {
      if (!explicitConversion || SourceInfo.ShapeKind != AR_TOBJ_SCALAR) {
      return false;
    }
    }

    // Assignment is valid if elements are exactly the same in type and size; if
    // an explicit conversion is being done, we accept converted elements and a
    // longer right-hand sequence.
    if (!explicitConversion &&
        (!result.AreElementsEqual || result.IsRightLonger()))
    {
      return false;
    }
    Second = ICK_Flat_Conversion;
    goto lSuccess;
  }

  // Convert scalar/vector/matrix dimensions
  if (!ConvertDimensions(TargetInfo, SourceInfo, Second, Remarks))
    return false;

  // Convert component type
  if (!ConvertComponent(TargetInfo, SourceInfo, ComponentConversion, Remarks))
    return false;

lSuccess:
  if (standard)
  {
    if (sourceExpr->isLValue())
    {
      if (needsLValueToRValue) {
        // We don't need LValueToRValue cast before casting a derived object
        // to its base.
        if (Second == ICK_HLSL_Derived_To_Base) {
          standard->First = ICK_Identity;
        } else {
          standard->First = ICK_Lvalue_To_Rvalue;
        }
      } else {
        switch (Second)
        {
        case ICK_NoReturn_Adjustment:
        case ICK_Vector_Conversion:
        case ICK_Vector_Splat:
          DXASSERT(false, "We shouldn't be producing these implicit conversion kinds");

        case ICK_Flat_Conversion:
        case ICK_HLSLVector_Splat:
          standard->First = ICK_Lvalue_To_Rvalue;
          break;
        default:
          // Only flat and splat conversions handled.
          break;
        }
        switch (ComponentConversion)
        {
        case ICK_Integral_Promotion:
        case ICK_Integral_Conversion:
        case ICK_Floating_Promotion:
        case ICK_Floating_Conversion:
        case ICK_Floating_Integral:
        case ICK_Boolean_Conversion:
          standard->First = ICK_Lvalue_To_Rvalue;
          break;
        case ICK_Array_To_Pointer:
          standard->First = ICK_Array_To_Pointer;
          break;
        default:
          // Only potential assignments above covered.
          break;
        }
      }
    }

    // Finally fix up the cases for scalar->scalar component conversion, and
    // identity vector/matrix component conversion
    if (ICK_Identity != ComponentConversion) {
      if (Second == ICK_Identity) {
        if (TargetInfo.ShapeKind == AR_TOBJ_BASIC) {
          // Scalar to scalar type conversion, use normal mechanism (Second)
          Second = ComponentConversion;
          ComponentConversion = ICK_Identity;
        }
        else if (TargetInfo.ShapeKind != AR_TOBJ_STRING) {
          // vector or matrix dimensions are not being changed, but component type
          // is being converted, so change Second to signal the conversion
          Second = ICK_HLSLVector_Conversion;
        }
      }
    }

    standard->Second = Second;
    standard->ComponentConversion = ComponentConversion;

    // For conversion which change to RValue but targeting reference type
    // Hold the conversion to codeGen
    if (targetRef && standard->First == ICK_Lvalue_To_Rvalue) {
      standard->First = ICK_Identity;
      standard->Second = ICK_Identity;
    }
  }

  AssignOpt(Remarks, remarks);
  return true;
}

bool HLSLExternalSource::ValidateTypeRequirements(
  SourceLocation loc,
  ArBasicKind elementKind,
  ArTypeObjectKind objectKind,
  bool requiresIntegrals,
  bool requiresNumerics)
{
  if (requiresIntegrals || requiresNumerics)
  {
    if (!IsObjectKindPrimitiveAggregate(objectKind))
    {
      m_sema->Diag(loc, diag::err_hlsl_requires_non_aggregate);
      return false;
    }
  }

  if (requiresIntegrals)
  {
    if (!IsBasicKindIntegral(elementKind))
    {
      m_sema->Diag(loc, diag::err_hlsl_requires_int_or_uint);
      return false;
    }
  }
  else if (requiresNumerics)
  {
    if (!IsBasicKindNumeric(elementKind))
    {
      m_sema->Diag(loc, diag::err_hlsl_requires_numeric);
      return false;
    }
  }

  return true;
}

bool HLSLExternalSource::ValidatePrimitiveTypeForOperand(SourceLocation loc, QualType type, ArTypeObjectKind kind)
{
  bool isValid = true;
  if (IsBuiltInObjectType(type)) {
    m_sema->Diag(loc, diag::err_hlsl_unsupported_builtin_op) << type;
    isValid = false;
  }
  if (kind == AR_TOBJ_COMPOUND) {
    m_sema->Diag(loc, diag::err_hlsl_unsupported_struct_op) << type;
    isValid = false;
  }
  return isValid;
}

HRESULT HLSLExternalSource::CombineDimensions(
  QualType leftType, QualType rightType, QualType *resultType,
  ImplicitConversionKind &convKind, TYPE_CONVERSION_REMARKS &Remarks)
{
  ArTypeInfo leftInfo, rightInfo;
  CollectInfo(leftType, &leftInfo);
  CollectInfo(rightType, &rightInfo);

  // Prefer larger, or left if same.
  if (leftInfo.uTotalElts >= rightInfo.uTotalElts) {
    if (ConvertDimensions(leftInfo, rightInfo, convKind, Remarks))
      *resultType = leftType;
    else if (ConvertDimensions(rightInfo, leftInfo, convKind, Remarks))
      *resultType = rightType;
    else
      return E_FAIL;
  } else {
    if (ConvertDimensions(rightInfo, leftInfo, convKind, Remarks))
      *resultType = rightType;
    else if (ConvertDimensions(leftInfo, rightInfo, convKind, Remarks))
      *resultType = leftType;
    else
      return E_FAIL;
  }

  return S_OK;
}

/// <summary>Validates and adjusts operands for the specified binary operator.</summary>
/// <param name="OpLoc">Source location for operator.</param>
/// <param name="Opc">Kind of binary operator.</param>
/// <param name="LHS">Left-hand-side expression, possibly updated by this function.</param>
/// <param name="RHS">Right-hand-side expression, possibly updated by this function.</param>
/// <param name="ResultTy">Result type for operator expression.</param>
/// <param name="CompLHSTy">Type of LHS after promotions for computation.</param>
/// <param name="CompResultTy">Type of computation result.</param>
void HLSLExternalSource::CheckBinOpForHLSL(
  SourceLocation OpLoc,
  BinaryOperatorKind Opc,
  ExprResult& LHS,
  ExprResult& RHS,
  QualType& ResultTy,
  QualType& CompLHSTy,
  QualType& CompResultTy)
{
  // At the start, none of the output types should be valid.
  DXASSERT_NOMSG(ResultTy.isNull());
  DXASSERT_NOMSG(CompLHSTy.isNull());
  DXASSERT_NOMSG(CompResultTy.isNull());

  LHS = m_sema->CorrectDelayedTyposInExpr(LHS);
  RHS = m_sema->CorrectDelayedTyposInExpr(RHS);

  // If either expression is invalid to begin with, propagate that.
  if (LHS.isInvalid() || RHS.isInvalid()) {
    return;
  }

  // TODO: re-review the Check** in Clang and add equivalent diagnostics if/as needed, possibly after conversions

  // Handle Assign and Comma operators and return
  switch (Opc)
  {
  case BO_AddAssign:
  case BO_AndAssign:
  case BO_DivAssign:
  case BO_MulAssign:
  case BO_RemAssign:
  case BO_ShlAssign:
  case BO_ShrAssign:
  case BO_SubAssign:
  case BO_OrAssign:
  case BO_XorAssign: {
    extern bool CheckForModifiableLvalue(Expr * E, SourceLocation Loc,
                                         Sema & S);
    if (CheckForModifiableLvalue(LHS.get(), OpLoc, *m_sema)) {
      return;
    }
  } break;
  case BO_Assign: {
      extern bool CheckForModifiableLvalue(Expr *E, SourceLocation Loc, Sema &S);
      if (CheckForModifiableLvalue(LHS.get(), OpLoc, *m_sema)) {
        return;
      }
      bool complained = false;
      ResultTy = LHS.get()->getType();
      if (m_sema->DiagnoseAssignmentResult(Sema::AssignConvertType::Compatible,
          OpLoc, ResultTy, RHS.get()->getType(), RHS.get(), 
          Sema::AssignmentAction::AA_Assigning, &complained)) {
        return;
      }
      StandardConversionSequence standard;
      if (!ValidateCast(OpLoc, RHS.get(), ResultTy, 
          ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
        return;
      }
      if (RHS.get()->isLValue()) {
        standard.First = ICK_Lvalue_To_Rvalue;
      }
      RHS = m_sema->PerformImplicitConversion(RHS.get(), ResultTy, 
        standard, Sema::AA_Converting, Sema::CCK_ImplicitConversion);
      return;
    }
    break;
  case BO_Comma:
    // C performs conversions, C++ doesn't but still checks for type completeness.
    // There are also diagnostics for improper comma use.
    // In the HLSL case these cases don't apply or simply aren't surfaced.
    ResultTy = RHS.get()->getType();
    return;
  default:
    // Only assign and comma operations handled.
    break;
  }

  // Leave this diagnostic for last to emulate fxc behavior.
  bool isCompoundAssignment = BinaryOperatorKindIsCompoundAssignment(Opc);
  bool unsupportedBoolLvalue = isCompoundAssignment && 
      !BinaryOperatorKindIsCompoundAssignmentForBool(Opc) &&
    GetTypeElementKind(LHS.get()->getType()) == AR_BASIC_BOOL;

  // Turn operand inputs into r-values.
  QualType LHSTypeAsPossibleLValue = LHS.get()->getType();
  if (!isCompoundAssignment) {
    LHS = m_sema->DefaultLvalueConversion(LHS.get());
  }
  RHS = m_sema->DefaultLvalueConversion(RHS.get());
  if (LHS.isInvalid() || RHS.isInvalid()) {
    return;
  }

  // Gather type info
  QualType leftType = GetStructuralForm(LHS.get()->getType());
  QualType rightType = GetStructuralForm(RHS.get()->getType());
  ArBasicKind leftElementKind = GetTypeElementKind(leftType);
  ArBasicKind rightElementKind = GetTypeElementKind(rightType);
  ArTypeObjectKind leftObjectKind = GetTypeObjectKind(leftType);
  ArTypeObjectKind rightObjectKind = GetTypeObjectKind(rightType);

  // Validate type requirements
  {
    bool requiresNumerics = BinaryOperatorKindRequiresNumeric(Opc);
    bool requiresIntegrals = BinaryOperatorKindRequiresIntegrals(Opc);

    if (!ValidateTypeRequirements(OpLoc, leftElementKind, leftObjectKind, requiresIntegrals, requiresNumerics)) {
      return;
    }
    if (!ValidateTypeRequirements(OpLoc, rightElementKind, rightObjectKind, requiresIntegrals, requiresNumerics)) {
      return;
    }
  }

  if (unsupportedBoolLvalue) {
    m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_bool_lvalue_op);
    return;
  }

  // We don't support binary operators on built-in object types other than assignment or commas.
  {
    DXASSERT(Opc != BO_Assign, "otherwise this wasn't handled as an early exit");
    DXASSERT(Opc != BO_Comma, "otherwise this wasn't handled as an early exit");
    bool isValid;
    isValid = ValidatePrimitiveTypeForOperand(OpLoc, leftType, leftObjectKind);
    if (leftType != rightType && !ValidatePrimitiveTypeForOperand(OpLoc, rightType, rightObjectKind)) {
      isValid = false;
    }
    if (!isValid) {
      return;
    }
  }

  // We don't support equality comparisons on arrays.
  if ((Opc == BO_EQ || Opc == BO_NE) && (leftObjectKind == AR_TOBJ_ARRAY || rightObjectKind == AR_TOBJ_ARRAY)) {
    m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_array_equality_op);
    return;
  }

  // Combine element types for computation.
  ArBasicKind resultElementKind = leftElementKind;
  {
    if (BinaryOperatorKindIsLogical(Opc)) {
      resultElementKind = AR_BASIC_BOOL;
    } else if (!BinaryOperatorKindIsBitwiseShift(Opc) && leftElementKind != rightElementKind) {
      if (!CombineBasicTypes(leftElementKind, rightElementKind, &resultElementKind)) {
        m_sema->Diag(OpLoc, diag::err_hlsl_type_mismatch);
        return;
      }
    } else if (BinaryOperatorKindIsBitwiseShift(Opc) &&
               (resultElementKind == AR_BASIC_LITERAL_INT ||
                resultElementKind == AR_BASIC_LITERAL_FLOAT) &&
               rightElementKind != AR_BASIC_LITERAL_INT &&
               rightElementKind != AR_BASIC_LITERAL_FLOAT) {
      // For case like 1<<x.
      resultElementKind = AR_BASIC_UINT32;
    } else if (resultElementKind == AR_BASIC_BOOL &&
               BinaryOperatorKindRequiresBoolAsNumeric(Opc)) {
      resultElementKind = AR_BASIC_INT32;
    }

    // The following combines the selected/combined element kind above with 
    // the dimensions that are legal to implicitly cast.  This means that
    // element kind may be taken from one side and the dimensions from the
    // other.

    if (!isCompoundAssignment) {
      // Legal dimension combinations are identical, splat, and truncation.
      // ResultTy will be set to whichever type can be converted to, if legal,
      // with preference for leftType if both are possible.
      if (FAILED(CombineDimensions(LHS.get()->getType(), RHS.get()->getType(), &ResultTy))) {
        // Just choose leftType, and allow ValidateCast to catch this later
        ResultTy = LHS.get()->getType();
      }
    } else {
      ResultTy = LHS.get()->getType();
    }

    // Here, element kind is combined with dimensions for computation type, if different.
    if (resultElementKind != GetTypeElementKind(ResultTy)) {
      UINT rowCount, colCount;
      GetRowsAndColsForAny(ResultTy, rowCount, colCount);
      ResultTy = NewSimpleAggregateType(GetTypeObjectKind(ResultTy), resultElementKind, 0, rowCount, colCount);
    }
  }

  bool bFailedFirstRHSCast = false;

  // Perform necessary conversion sequences for LHS and RHS
  if (RHS.get()->getType() != ResultTy) {
    StandardConversionSequence standard;
    // Suppress type narrowing or truncation warnings for RHS on bitwise shift, since we only care about the LHS type.
    bool bSuppressWarnings = BinaryOperatorKindIsBitwiseShift(Opc);
    // Suppress errors on compound assignment, since we will vaildate the cast to the final type later.
    bool bSuppressErrors = isCompoundAssignment;
    // If compound assignment, suppress errors until later, but report warning (vector truncation/type narrowing) here.
    if (ValidateCast(SourceLocation(), RHS.get(), ResultTy, ExplicitConversionFalse, bSuppressWarnings, bSuppressErrors, &standard)) {
      if (standard.First != ICK_Identity || !standard.isIdentityConversion())
        RHS = m_sema->PerformImplicitConversion(RHS.get(), ResultTy, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    } else if (!isCompoundAssignment) {
      // If compound assignment, validate cast from RHS directly to LHS later, otherwise, fail here.
      ResultTy = QualType();
      return;
    } else {
      bFailedFirstRHSCast = true;
    }
  }

  if (isCompoundAssignment) {
    CompResultTy = ResultTy;
    CompLHSTy = CompResultTy;

    // For a compound operation, C/C++ promotes both types, performs the arithmetic,
    // then converts to the result type and then assigns.
    //
    // So int + float promotes the int to float, does a floating-point addition,
    // then the result becomes and int and is assigned.
    ResultTy = LHSTypeAsPossibleLValue;

    // Validate remainder of cast from computation type to final result type
    StandardConversionSequence standard;
    if (!ValidateCast(SourceLocation(), RHS.get(), ResultTy, ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
      ResultTy = QualType();
      return;
    }
    DXASSERT_LOCALVAR(bFailedFirstRHSCast, !bFailedFirstRHSCast,
      "otherwise, hit compound assign case that failed RHS -> CompResultTy cast, but succeeded RHS -> LHS cast.");

  } else if (LHS.get()->getType() != ResultTy) {
    StandardConversionSequence standard;
    if (ValidateCast(SourceLocation(), LHS.get(), ResultTy, ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
      if (standard.First != ICK_Identity || !standard.isIdentityConversion())
        LHS = m_sema->PerformImplicitConversion(LHS.get(), ResultTy, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    } else {
      ResultTy = QualType();
      return;
    }
  }

  if (BinaryOperatorKindIsComparison(Opc) || BinaryOperatorKindIsLogical(Opc))
  {
    DXASSERT(!isCompoundAssignment, "otherwise binary lookup tables are inconsistent");
    // Return bool vector for vector types.
    if (IsVectorType(m_sema, ResultTy)) {
      UINT rowCount, colCount;
      GetRowsAndColsForAny(ResultTy, rowCount, colCount);
      ResultTy = LookupVectorType(HLSLScalarType::HLSLScalarType_bool, colCount);
    } else if (IsMatrixType(m_sema, ResultTy)) {
      UINT rowCount, colCount;
      GetRowsAndColsForAny(ResultTy, rowCount, colCount);
      ResultTy = LookupMatrixType(HLSLScalarType::HLSLScalarType_bool, rowCount, colCount);
    } else
      ResultTy = m_context->BoolTy.withConst();
  }

  // Run diagnostics. Some are emulating checks that occur in IR emission in fxc.
  if (Opc == BO_Div || Opc == BO_DivAssign || Opc == BO_Rem || Opc == BO_RemAssign) {
    if (IsBasicKindIntMinPrecision(resultElementKind)) {
      m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_div_minint);
      return;
    }
  }

  if (Opc == BO_Rem || Opc == BO_RemAssign) {
    if (resultElementKind == AR_BASIC_FLOAT64) {
      m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_mod_double);
      return;
    }
  }
}

/// <summary>Validates and adjusts operands for the specified unary operator.</summary>
/// <param name="OpLoc">Source location for operator.</param>
/// <param name="Opc">Kind of operator.</param>
/// <param name="InputExpr">Input expression to the operator.</param>
/// <param name="VK">Value kind for resulting expression.</param>
/// <param name="OK">Object kind for resulting expression.</param>
/// <returns>The result type for the expression.</returns>
QualType HLSLExternalSource::CheckUnaryOpForHLSL(
  SourceLocation OpLoc,
  UnaryOperatorKind Opc,
  ExprResult& InputExpr,
  ExprValueKind& VK,
  ExprObjectKind& OK)
{
  InputExpr = m_sema->CorrectDelayedTyposInExpr(InputExpr);

  if (InputExpr.isInvalid())
    return QualType();

  // Reject unsupported operators * and &
  switch (Opc) {
  case UO_AddrOf:
  case UO_Deref:
    m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_operator);
    return QualType();
  default:
    // Only * and & covered.
    break;
  }

  Expr* expr = InputExpr.get();
  if (expr->isTypeDependent())
    return m_context->DependentTy;

  ArBasicKind elementKind = GetTypeElementKind(expr->getType());

  if (UnaryOperatorKindRequiresModifiableValue(Opc)) {
    if (elementKind == AR_BASIC_ENUM) {
      bool isInc = IsIncrementOp(Opc);
      m_sema->Diag(OpLoc, diag::err_increment_decrement_enum) << isInc << expr->getType();
      return QualType();
    }

    extern bool CheckForModifiableLvalue(Expr *E, SourceLocation Loc, Sema &S);
    if (CheckForModifiableLvalue(expr, OpLoc, *m_sema))
      return QualType();
  } else {
    InputExpr = m_sema->DefaultLvalueConversion(InputExpr.get()).get();
    if (InputExpr.isInvalid()) return QualType();
  }

  if (UnaryOperatorKindDisallowsBool(Opc) && IS_BASIC_BOOL(elementKind)) {
    m_sema->Diag(OpLoc, diag::err_hlsl_unsupported_bool_lvalue_op);
    return QualType();
  }

  if (UnaryOperatorKindRequiresBoolAsNumeric(Opc)) {
    InputExpr = PromoteToIntIfBool(InputExpr);
    expr = InputExpr.get();
    elementKind = GetTypeElementKind(expr->getType());
  }

  ArTypeObjectKind objectKind = GetTypeObjectKind(expr->getType());
  bool requiresIntegrals = UnaryOperatorKindRequiresIntegrals(Opc);
  bool requiresNumerics = UnaryOperatorKindRequiresNumerics(Opc);
  if (!ValidateTypeRequirements(OpLoc, elementKind, objectKind, requiresIntegrals, requiresNumerics)) {
    return QualType();
  }

  if (Opc == UnaryOperatorKind::UO_Minus) {
    if (IS_BASIC_UINT(Opc)) {
      m_sema->Diag(OpLoc, diag::warn_hlsl_unary_negate_unsigned);
    }
  }

  // By default, the result type is the operand type.
  // Logical not however should cast to a bool.
  QualType resultType = expr->getType();
  if (Opc == UnaryOperatorKind::UO_LNot) {
    UINT rowCount, colCount;
    GetRowsAndColsForAny(expr->getType(), rowCount, colCount);
    resultType = NewSimpleAggregateType(objectKind, AR_BASIC_BOOL, AR_QUAL_CONST, rowCount, colCount);
    StandardConversionSequence standard;
    if (!CanConvert(OpLoc, expr, resultType, false, nullptr, &standard)) {
      m_sema->Diag(OpLoc, diag::err_hlsl_requires_bool_for_not);
      return QualType();
    }

    // Cast argument.
    ExprResult result = m_sema->PerformImplicitConversion(InputExpr.get(), resultType, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    if (result.isUsable()) {
      InputExpr = result.get();
    }
  }

  bool isPrefix = Opc == UO_PreInc || Opc == UO_PreDec;
  if (isPrefix) {
    VK = VK_LValue;
    return resultType;
  }
  else {
    VK = VK_RValue;
    return resultType.getUnqualifiedType();
  }
}

clang::QualType HLSLExternalSource::CheckVectorConditional(
  _In_ ExprResult &Cond,
  _In_ ExprResult &LHS,
  _In_ ExprResult &RHS,
  _In_ SourceLocation QuestionLoc)
{

  Cond = m_sema->CorrectDelayedTyposInExpr(Cond);
  LHS = m_sema->CorrectDelayedTyposInExpr(LHS);
  RHS = m_sema->CorrectDelayedTyposInExpr(RHS);

  // If either expression is invalid to begin with, propagate that.
  if (Cond.isInvalid() || LHS.isInvalid() || RHS.isInvalid()) {
    return QualType();
  }

  // Gather type info
  QualType condType = GetStructuralForm(Cond.get()->getType());
  QualType leftType = GetStructuralForm(LHS.get()->getType());
  QualType rightType = GetStructuralForm(RHS.get()->getType());
  ArBasicKind condElementKind = GetTypeElementKind(condType);
  ArBasicKind leftElementKind = GetTypeElementKind(leftType);
  ArBasicKind rightElementKind = GetTypeElementKind(rightType);
  ArTypeObjectKind condObjectKind = GetTypeObjectKind(condType);
  ArTypeObjectKind leftObjectKind = GetTypeObjectKind(leftType);
  ArTypeObjectKind rightObjectKind = GetTypeObjectKind(rightType);

  QualType ResultTy = leftType;

  bool condIsSimple = condObjectKind == AR_TOBJ_BASIC || condObjectKind == AR_TOBJ_VECTOR || condObjectKind == AR_TOBJ_MATRIX;
  if (!condIsSimple) {
    m_sema->Diag(QuestionLoc, diag::err_hlsl_conditional_cond_typecheck);
    return QualType();
  }

  UINT rowCountCond, colCountCond;
  GetRowsAndColsForAny(condType, rowCountCond, colCountCond);

  bool leftIsSimple =
      leftObjectKind == AR_TOBJ_BASIC || leftObjectKind == AR_TOBJ_VECTOR ||
      leftObjectKind == AR_TOBJ_MATRIX;
  bool rightIsSimple =
      rightObjectKind == AR_TOBJ_BASIC || rightObjectKind == AR_TOBJ_VECTOR ||
      rightObjectKind == AR_TOBJ_MATRIX;

  if (!leftIsSimple || !rightIsSimple) {
    if (leftObjectKind == AR_TOBJ_OBJECT && leftObjectKind == AR_TOBJ_OBJECT) {
      if (leftType == rightType) {
        return leftType;
      }
    }
    // NOTE: Limiting this operator to working only on basic numeric types.
    // This is due to extremely limited (and even broken) support for any other case.
    // In the future we may decide to support more cases.
    m_sema->Diag(QuestionLoc, diag::err_hlsl_conditional_result_typecheck);
    return QualType();
  }

  // Types should be only scalar, vector, or matrix after this point.

  ArBasicKind resultElementKind = leftElementKind;
  // Combine LHS and RHS element types for computation.
  if (leftElementKind != rightElementKind) {
    if (!CombineBasicTypes(leftElementKind, rightElementKind, &resultElementKind)) {
      m_sema->Diag(QuestionLoc, diag::err_hlsl_conditional_result_comptype_mismatch);
      return QualType();
    }
  }

  // Restore left/right type to original to avoid stripping attributed type or typedef type
  leftType = LHS.get()->getType();
  rightType = RHS.get()->getType();

  // Combine LHS and RHS dimensions
  if (FAILED(CombineDimensions(leftType, rightType, &ResultTy))) {
    m_sema->Diag(QuestionLoc, diag::err_hlsl_conditional_result_dimensions);
    return QualType();
  }

  UINT rowCount, colCount;
  GetRowsAndColsForAny(ResultTy, rowCount, colCount);

  // If result is scalar, use condition dimensions.
  // Otherwise, condition must either match or is scalar, then use result dimensions
  if (rowCount * colCount == 1) {
    rowCount = rowCountCond;
    colCount = colCountCond;
  }
  else if (rowCountCond * colCountCond != 1 && (rowCountCond != rowCount || colCountCond != colCount)) {
    m_sema->Diag(QuestionLoc, diag::err_hlsl_conditional_dimensions);
    return QualType();
  }

  // Here, element kind is combined with dimensions for result type.
  ResultTy = NewSimpleAggregateType(AR_TOBJ_INVALID, resultElementKind, 0, rowCount, colCount)->getCanonicalTypeInternal();

  // Cast condition to RValue
  if (Cond.get()->isLValue())
    Cond.set(CreateLValueToRValueCast(Cond.get()));

  // Convert condition component type to bool, using result component dimensions
  if (condElementKind != AR_BASIC_BOOL) {
    QualType boolType = NewSimpleAggregateType(AR_TOBJ_INVALID, AR_BASIC_BOOL, 0, rowCount, colCount)->getCanonicalTypeInternal();
    StandardConversionSequence standard;
    if (ValidateCast(SourceLocation(), Cond.get(), boolType, ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
      if (standard.First != ICK_Identity || !standard.isIdentityConversion())
        Cond = m_sema->PerformImplicitConversion(Cond.get(), boolType, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    }
    else {
      return QualType();
    }
  }

  // TODO: Is this correct?  Does fxc support lvalue return here?
  // Cast LHS/RHS to RValue
  if (LHS.get()->isLValue())
    LHS.set(CreateLValueToRValueCast(LHS.get()));
  if (RHS.get()->isLValue())
    RHS.set(CreateLValueToRValueCast(RHS.get()));

  if (leftType != ResultTy) {
    StandardConversionSequence standard;
    if (ValidateCast(SourceLocation(), LHS.get(), ResultTy, ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
      if (standard.First != ICK_Identity || !standard.isIdentityConversion())
        LHS = m_sema->PerformImplicitConversion(LHS.get(), ResultTy, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    }
    else {
      return QualType();
    }
  }
  if (rightType != ResultTy) {
    StandardConversionSequence standard;
    if (ValidateCast(SourceLocation(), RHS.get(), ResultTy, ExplicitConversionFalse, SuppressWarningsFalse, SuppressErrorsFalse, &standard)) {
      if (standard.First != ICK_Identity || !standard.isIdentityConversion())
        RHS = m_sema->PerformImplicitConversion(RHS.get(), ResultTy, standard, Sema::AA_Casting, Sema::CCK_ImplicitConversion);
    }
    else {
      return QualType();
    }
  }

  return ResultTy;
}

// Apply type specifier sign to the given QualType.
// Other than privmitive int type, only allow shorthand vectors and matrices to be unsigned.
clang::QualType HLSLExternalSource::ApplyTypeSpecSignToParsedType(
    _In_ clang::QualType &type, _In_ clang::TypeSpecifierSign TSS,
    _In_ clang::SourceLocation Loc) {
  if (TSS == TypeSpecifierSign::TSS_unspecified) {
    return type;
  }
  DXASSERT(TSS != TypeSpecifierSign::TSS_signed, "else signed keyword is supported in HLSL");
  ArTypeObjectKind objKind = GetTypeObjectKind(type);
  if (objKind != AR_TOBJ_VECTOR && objKind != AR_TOBJ_MATRIX &&
      objKind != AR_TOBJ_BASIC && objKind != AR_TOBJ_ARRAY) {
    return type;
  }
  // check if element type is unsigned and check if such vector exists
  // If not create a new one, Make a QualType of the new kind
  ArBasicKind elementKind = GetTypeElementKind(type);
  // Only ints can have signed/unsigend ty
  if (!IS_BASIC_UNSIGNABLE(elementKind)) {
    return type;
  }
  else {
    // Check given TypeSpecifierSign. If unsigned, change int to uint.
    HLSLScalarType scalarType = ScalarTypeForBasic(elementKind);
    HLSLScalarType newScalarType = MakeUnsigned(scalarType);

    // Get new vector types for a given TypeSpecifierSign.
    if (objKind == AR_TOBJ_VECTOR) {
      UINT colCount = GetHLSLVecSize(type);
      TypedefDecl *qts = LookupVectorShorthandType(newScalarType, colCount);
      return m_context->getTypeDeclType(qts);
    } else if (objKind == AR_TOBJ_MATRIX) {
      UINT rowCount, colCount;
      GetRowsAndCols(type, rowCount, colCount);
      TypedefDecl *qts = LookupMatrixShorthandType(newScalarType, rowCount, colCount);
      return m_context->getTypeDeclType(qts);
    } else {
      DXASSERT_NOMSG(objKind == AR_TOBJ_BASIC || objKind == AR_TOBJ_ARRAY);
      return m_scalarTypes[newScalarType];
    }
  }
}

Sema::TemplateDeductionResult HLSLExternalSource::DeduceTemplateArgumentsForHLSL(
  FunctionTemplateDecl *FunctionTemplate,
  TemplateArgumentListInfo *ExplicitTemplateArgs, ArrayRef<Expr *> Args,
  FunctionDecl *&Specialization, TemplateDeductionInfo &Info)
{
  DXASSERT_NOMSG(FunctionTemplate != nullptr);

  // Get information about the function we have.
  CXXMethodDecl* functionMethod = dyn_cast<CXXMethodDecl>(FunctionTemplate->getTemplatedDecl());
  DXASSERT(functionMethod != nullptr,
    "otherwise this is standalone function rather than a method, which isn't supported in the HLSL object model");
  CXXRecordDecl* functionParentRecord = functionMethod->getParent();
  DXASSERT(functionParentRecord != nullptr, "otherwise function is orphaned");
  QualType objectElement = GetFirstElementTypeFromDecl(functionParentRecord);

  QualType functionTemplateTypeArg {};
  if (ExplicitTemplateArgs != nullptr && ExplicitTemplateArgs->size() == 1) {
    const TemplateArgument &firstTemplateArg = (*ExplicitTemplateArgs)[0].getArgument();
    if (firstTemplateArg.getKind() == TemplateArgument::ArgKind::Type)
      functionTemplateTypeArg = firstTemplateArg.getAsType();
  }

  // Handle subscript overloads.
  if (FunctionTemplate->getDeclName() == m_context->DeclarationNames.getCXXOperatorName(OO_Subscript))
  {
    DeclContext* functionTemplateContext = FunctionTemplate->getDeclContext();
    FindStructBasicTypeResult findResult = FindStructBasicType(functionTemplateContext);
    if (!findResult.Found())
    {
      // This might be a nested type. Do a lookup on the parent.
      CXXRecordDecl* parentRecordType = dyn_cast_or_null<CXXRecordDecl>(functionTemplateContext);
      if (parentRecordType == nullptr || parentRecordType->getDeclContext() == nullptr)
      {
        return Sema::TemplateDeductionResult::TDK_Invalid;
      }

      findResult = FindStructBasicType(parentRecordType->getDeclContext());
      if (!findResult.Found())
      {
        return Sema::TemplateDeductionResult::TDK_Invalid;
      }

      DXASSERT(
        parentRecordType->getDeclContext()->getDeclKind() == Decl::Kind::CXXRecord ||
        parentRecordType->getDeclContext()->getDeclKind() == Decl::Kind::ClassTemplateSpecialization,
        "otherwise FindStructBasicType should have failed - no other types are allowed");
      objectElement = GetFirstElementTypeFromDecl(
        cast<CXXRecordDecl>(parentRecordType->getDeclContext()));
    }

    Specialization = AddSubscriptSpecialization(FunctionTemplate, objectElement, findResult);
    DXASSERT_NOMSG(Specialization->getPrimaryTemplate()->getCanonicalDecl() ==
      FunctionTemplate->getCanonicalDecl());

    return Sema::TemplateDeductionResult::TDK_Success;
  }

  // Reject overload lookups that aren't identifier-based.
  if (!FunctionTemplate->getDeclName().isIdentifier())
  {
    return Sema::TemplateDeductionResult::TDK_NonDeducedMismatch;
  }

  // Find the table of intrinsics based on the object type.
  const HLSL_INTRINSIC* intrinsics = nullptr;
  size_t intrinsicCount = 0;
  const char* objectName = nullptr;
  FindIntrinsicTable(FunctionTemplate->getDeclContext(), &objectName, &intrinsics, &intrinsicCount);
  DXASSERT(objectName != nullptr &&
    (intrinsics != nullptr || m_intrinsicTables.size() > 0),
    "otherwise FindIntrinsicTable failed to lookup a valid object, "
    "or the parser let a user-defined template object through");

  // Look for an intrinsic for which we can match arguments.
  size_t argCount;
  QualType argTypes[g_MaxIntrinsicParamCount + 1];
  StringRef nameIdentifier = FunctionTemplate->getName();
  IntrinsicDefIter cursor = FindIntrinsicByNameAndArgCount(intrinsics, intrinsicCount, objectName, nameIdentifier, Args.size());
  IntrinsicDefIter end = IntrinsicDefIter::CreateEnd(intrinsics, intrinsicCount, IntrinsicTableDefIter::CreateEnd(m_intrinsicTables));

  while (cursor != end)
  {
    if (!MatchArguments(*cursor, objectElement, functionTemplateTypeArg, Args, argTypes, &argCount))
    {
      ++cursor;
      continue;
    }

    // Currently only intrinsic we allow for explicit template arguments are
    // for Load/Store for ByteAddressBuffer/RWByteAddressBuffer
    // TODO: handle template arguments for future intrinsics in a more natural way

    // Check Explicit template arguments
    UINT intrinsicOp = (*cursor)->Op;
    LPCSTR intrinsicName = (*cursor)->pArgs[0].pName;
    bool Is2018 = getSema()->getLangOpts().HLSLVersion >= 2018;
    bool IsBAB =
        objectName == g_ArBasicTypeNames[AR_OBJECT_BYTEADDRESS_BUFFER] ||
        objectName == g_ArBasicTypeNames[AR_OBJECT_RWBYTEADDRESS_BUFFER];
    bool IsBABLoad = IsBAB && intrinsicOp == (UINT)IntrinsicOp::MOP_Load;
    bool IsBABStore = IsBAB && intrinsicOp == (UINT)IntrinsicOp::MOP_Store;
    if (ExplicitTemplateArgs && ExplicitTemplateArgs->size() > 0) {
      bool isLegalTemplate = false;
      SourceLocation Loc = ExplicitTemplateArgs->getLAngleLoc();
      auto TemplateDiag = diag::err_hlsl_intrinsic_template_arg_unsupported;
      if (ExplicitTemplateArgs->size() >= 1 && (IsBABLoad || IsBABStore)) {
        TemplateDiag = diag::err_hlsl_intrinsic_template_arg_requires_2018;
        Loc = (*ExplicitTemplateArgs)[0].getLocation();
        if (Is2018) {
          TemplateDiag = diag::err_hlsl_intrinsic_template_arg_numeric;
          if (ExplicitTemplateArgs->size() == 1
              && !functionTemplateTypeArg.isNull()
              && hlsl::IsHLSLNumericOrAggregateOfNumericType(functionTemplateTypeArg)) {
            isLegalTemplate = true;
            argTypes[0] = functionTemplateTypeArg;
          }
        }
      }

      if (!isLegalTemplate) {
        getSema()->Diag(Loc, TemplateDiag) << intrinsicName;
        return Sema::TemplateDeductionResult::TDK_Invalid;
      }
    } else if (IsBABStore) {
      // Prior to HLSL 2018, Store operation only stored scalar uint.
      if (!Is2018) {
        if (GetNumElements(argTypes[2]) != 1) {
          getSema()->Diag(Args[1]->getLocStart(),
                          diag::err_ovl_no_viable_member_function_in_call)
              << intrinsicName;
          return Sema::TemplateDeductionResult::TDK_Invalid;
        }
        argTypes[2] = getSema()->getASTContext().getIntTypeForBitwidth(
            32, /*signed*/ false);
      }
    }
    Specialization = AddHLSLIntrinsicMethod(cursor.GetTableName(), cursor.GetLoweringStrategy(), *cursor, FunctionTemplate, Args, argTypes, argCount);
    DXASSERT_NOMSG(Specialization->getPrimaryTemplate()->getCanonicalDecl() ==
      FunctionTemplate->getCanonicalDecl());

    if (!IsValidateObjectElement(*cursor, objectElement)) {
      m_sema->Diag(Args[0]->getExprLoc(), diag::err_hlsl_invalid_resource_type_on_intrinsic) <<
          nameIdentifier << g_ArBasicTypeNames[GetTypeElementKind(objectElement)];
    }
    return Sema::TemplateDeductionResult::TDK_Success;
  }

  return Sema::TemplateDeductionResult::TDK_NonDeducedMismatch;
}

void HLSLExternalSource::ReportUnsupportedTypeNesting(SourceLocation loc, QualType type)
{
  m_sema->Diag(loc, diag::err_hlsl_unsupported_type_nesting) << type;
}

bool HLSLExternalSource::TryStaticCastForHLSL(ExprResult &SrcExpr,
  QualType DestType,
  Sema::CheckedConversionKind CCK,
  const SourceRange &OpRange, unsigned &msg,
  CastKind &Kind, CXXCastPath &BasePath,
  bool ListInitialization, bool SuppressWarnings, bool SuppressErrors,
  _Inout_opt_ StandardConversionSequence* standard)
{
  DXASSERT(!SrcExpr.isInvalid(), "caller should check for invalid expressions and placeholder types");
  bool explicitConversion
    = (CCK == Sema::CCK_CStyleCast || CCK == Sema::CCK_FunctionalCast);
  bool suppressWarnings = explicitConversion || SuppressWarnings;
  SourceLocation loc = OpRange.getBegin();
  if (ValidateCast(loc, SrcExpr.get(), DestType, explicitConversion, suppressWarnings, SuppressErrors, standard)) {
    // TODO: LValue to RValue cast was all that CanConvert (ValidateCast) did anyway, 
    // so do this here until we figure out why this is needed.
    if (standard && standard->First == ICK_Lvalue_To_Rvalue) {
      SrcExpr.set(CreateLValueToRValueCast(SrcExpr.get()));
    }
    return true;
  }

  // ValidateCast includes its own error messages.
  msg = 0;
  return false;
}

/// <summary>
/// Checks if a subscript index argument can be initialized from the given expression.
/// </summary>
/// <param name="SrcExpr">Source expression used as argument.</param>
/// <param name="DestType">Parameter type to initialize.</param>
/// <remarks>
/// Rules for subscript index initialization follow regular implicit casting rules, with the exception that
/// no changes in arity are allowed (i.e., int2 can become uint2, but uint or uint3 cannot).
/// </remarks>
ImplicitConversionSequence
HLSLExternalSource::TrySubscriptIndexInitialization(_In_ clang::Expr *SrcExpr,
                                                    clang::QualType DestType) {
  DXASSERT_NOMSG(SrcExpr != nullptr);
  DXASSERT_NOMSG(!DestType.isNull());

  unsigned int msg = 0;
  CastKind kind;
  CXXCastPath path;
  ImplicitConversionSequence sequence;
  sequence.setStandard();
  ExprResult sourceExpr(SrcExpr);
  if (GetElementCount(SrcExpr->getType()) != GetElementCount(DestType)) {
    sequence.setBad(BadConversionSequence::FailureKind::no_conversion,
                    SrcExpr->getType(), DestType);
  } else if (!TryStaticCastForHLSL(
                 sourceExpr, DestType, Sema::CCK_ImplicitConversion, NoRange,
                 msg, kind, path, ListInitializationFalse,
                 SuppressWarningsFalse, SuppressErrorsTrue, &sequence.Standard)) {
    sequence.setBad(BadConversionSequence::FailureKind::no_conversion,
                    SrcExpr->getType(), DestType);
  }

  return sequence;
}

template <typename T>
static
bool IsValueInRange(T value, T minValue, T maxValue) {
  return minValue <= value && value <= maxValue;
}

#define D3DX_16F_MAX          6.550400e+004    // max value
#define D3DX_16F_MIN          6.1035156e-5f    // min positive value

static
void GetFloatLimits(ArBasicKind basicKind, double* minValue, double* maxValue)
{
  DXASSERT_NOMSG(minValue != nullptr);
  DXASSERT_NOMSG(maxValue != nullptr);

  switch (basicKind) {
  case AR_BASIC_MIN10FLOAT:
  case AR_BASIC_MIN16FLOAT:
  case AR_BASIC_FLOAT16: *minValue = -(D3DX_16F_MIN); *maxValue = D3DX_16F_MAX; return;
  case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
  case AR_BASIC_FLOAT32: *minValue = -(FLT_MIN); *maxValue = FLT_MAX; return;
  case AR_BASIC_FLOAT64: *minValue = -(DBL_MIN); *maxValue = DBL_MAX; return;
  default:
    // No other float types.
    break;
  }

  DXASSERT(false, "unreachable");
  *minValue = 0; *maxValue = 0;
  return;
}

static
void GetUnsignedLimit(ArBasicKind basicKind, uint64_t* maxValue)
{
  DXASSERT_NOMSG(maxValue != nullptr);

  switch (basicKind) {
  case AR_BASIC_BOOL:   *maxValue = 1; return;
  case AR_BASIC_UINT8:  *maxValue = UINT8_MAX; return;
  case AR_BASIC_MIN16UINT:
  case AR_BASIC_UINT16: *maxValue = UINT16_MAX; return;
  case AR_BASIC_UINT32: *maxValue = UINT32_MAX; return;
  case AR_BASIC_UINT64: *maxValue = UINT64_MAX; return;
  default:
    // No other unsigned int types.
    break;
  }

  DXASSERT(false, "unreachable");
  *maxValue = 0;
  return;
}

static
void GetSignedLimits(ArBasicKind basicKind, int64_t* minValue, int64_t* maxValue)
{
  DXASSERT_NOMSG(minValue != nullptr);
  DXASSERT_NOMSG(maxValue != nullptr);

  switch (basicKind) {
  case AR_BASIC_INT8:  *minValue =  INT8_MIN; *maxValue =  INT8_MAX; return;
  case AR_BASIC_MIN12INT:
  case AR_BASIC_MIN16INT:
  case AR_BASIC_INT16: *minValue = INT16_MIN; *maxValue = INT16_MAX; return;
  case AR_BASIC_INT32: *minValue = INT32_MIN; *maxValue = INT32_MAX; return;
  case AR_BASIC_INT64: *minValue = INT64_MIN; *maxValue = INT64_MAX; return;
  default:
    // No other signed int types.
    break;
  }

  DXASSERT(false, "unreachable");
  *minValue = 0; *maxValue = 0;
  return;
}

static
bool IsValueInBasicRange(ArBasicKind basicKind, const APValue& value)
{
  if (IS_BASIC_FLOAT(basicKind)) {

    double val;
    if (value.isInt()) {
      val = value.getInt().getLimitedValue();
    } else if (value.isFloat()) {
      llvm::APFloat floatValue = value.getFloat();
      if (!floatValue.isFinite()) {
        return false;
      }
      llvm::APFloat valueFloat = value.getFloat();
      if (&valueFloat.getSemantics() == &llvm::APFloat::IEEEsingle) {
        val = value.getFloat().convertToFloat();
      }
      else {
        val = value.getFloat().convertToDouble();
      }
    } else {
      return false;
    }
    double minValue, maxValue;
    GetFloatLimits(basicKind, &minValue, &maxValue);
    return IsValueInRange(val, minValue, maxValue);
  }
  else if (IS_BASIC_SINT(basicKind)) {
    if (!value.isInt()) {
      return false;
    }
    int64_t val = value.getInt().getSExtValue();
    int64_t minValue, maxValue;
    GetSignedLimits(basicKind, &minValue, &maxValue);
    return IsValueInRange(val, minValue, maxValue);
  }
  else if (IS_BASIC_UINT(basicKind) || IS_BASIC_BOOL(basicKind)) {
    if (!value.isInt()) {
      return false;
    }
    uint64_t val = value.getInt().getLimitedValue();
    uint64_t maxValue;
    GetUnsignedLimit(basicKind, &maxValue);
    return IsValueInRange(val, (uint64_t)0, maxValue);
  }
  else {
    return false;
  }
}

static
bool IsPrecisionLossIrrelevant(ASTContext& Ctx, _In_ const Expr* sourceExpr, QualType targetType, ArBasicKind targetKind)
{
  DXASSERT_NOMSG(!targetType.isNull());
  DXASSERT_NOMSG(sourceExpr != nullptr);

  Expr::EvalResult evalResult;
  if (sourceExpr->EvaluateAsRValue(evalResult, Ctx)) {
    if (evalResult.Diag == nullptr || evalResult.Diag->empty()) {
      return IsValueInBasicRange(targetKind, evalResult.Val);
    }
  }

  return false;
}

bool HLSLExternalSource::ValidateCast(
  SourceLocation OpLoc,
  _In_ Expr* sourceExpr,
  QualType target,
  bool explicitConversion,
  bool suppressWarnings,
  bool suppressErrors,
  _Inout_opt_ StandardConversionSequence* standard)
{
  DXASSERT_NOMSG(sourceExpr != nullptr);

  if (OpLoc.isInvalid())
    OpLoc = sourceExpr->getExprLoc();

  QualType source = sourceExpr->getType();
  TYPE_CONVERSION_REMARKS remarks;
  if (!CanConvert(OpLoc, sourceExpr, target, explicitConversion, &remarks, standard))
  {
    const bool IsOutputParameter = false;

    //
    // Check whether the lack of explicit-ness matters.
    //
    // Setting explicitForDiagnostics to true in that case will avoid the message
    // saying anything about the implicit nature of the cast, when adding the
    // explicit cast won't make a difference.
    //
    bool explicitForDiagnostics = explicitConversion;
    if (explicitConversion == false)
    {
      if (!CanConvert(OpLoc, sourceExpr, target, true, &remarks, nullptr))
      {
        // Can't convert either way - implicit/explicit doesn't matter.
        explicitForDiagnostics = true;
      }
    }

    if (!suppressErrors)
    {
      m_sema->Diag(OpLoc, diag::err_hlsl_cannot_convert)
        << explicitForDiagnostics << IsOutputParameter << source << target;
    }
    return false;
  }

  if (!suppressWarnings)
  {
    if (!explicitConversion)
    {
      if ((remarks & TYPE_CONVERSION_PRECISION_LOSS) != 0)
      {
        // This is a much more restricted version of the analysis does
        // StandardConversionSequence::getNarrowingKind
        if (!IsPrecisionLossIrrelevant(*m_context, sourceExpr, target, GetTypeElementKind(target)))
        {
          m_sema->Diag(OpLoc, diag::warn_hlsl_narrowing) << source << target;
        }
      }

      if ((remarks & TYPE_CONVERSION_ELT_TRUNCATION) != 0)
      {
        m_sema->Diag(OpLoc, diag::warn_hlsl_implicit_vector_truncation);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Functions exported from this translation unit.                             //

/// <summary>Performs HLSL-specific processing for unary operators.</summary>
QualType hlsl::CheckUnaryOpForHLSL(Sema& self,
  SourceLocation OpLoc,
  UnaryOperatorKind Opc,
  ExprResult& InputExpr,
  ExprValueKind& VK,
  ExprObjectKind& OK)
{
  ExternalSemaSource* externalSource = self.getExternalSource();
  if (externalSource == nullptr) {
    return QualType();
  }

  HLSLExternalSource* hlsl = reinterpret_cast<HLSLExternalSource*>(externalSource);
  return hlsl->CheckUnaryOpForHLSL(OpLoc, Opc, InputExpr, VK, OK);
}

/// <summary>Performs HLSL-specific processing for binary operators.</summary>
void hlsl::CheckBinOpForHLSL(Sema& self,
  SourceLocation OpLoc,
  BinaryOperatorKind Opc,
  ExprResult& LHS,
  ExprResult& RHS,
  QualType& ResultTy,
  QualType& CompLHSTy,
  QualType& CompResultTy)
{
  ExternalSemaSource* externalSource = self.getExternalSource();
  if (externalSource == nullptr) {
    return;
  }

  HLSLExternalSource* hlsl = reinterpret_cast<HLSLExternalSource*>(externalSource);
  return hlsl->CheckBinOpForHLSL(OpLoc, Opc, LHS, RHS, ResultTy, CompLHSTy, CompResultTy);
}

/// <summary>Performs HLSL-specific processing of template declarations.</summary>
bool hlsl::CheckTemplateArgumentListForHLSL(Sema& self, TemplateDecl* Template, SourceLocation TemplateLoc, TemplateArgumentListInfo& TemplateArgList)
{
  DXASSERT_NOMSG(Template != nullptr);

  ExternalSemaSource* externalSource = self.getExternalSource();
  if (externalSource == nullptr) {
    return false;
  }

  HLSLExternalSource* hlsl = reinterpret_cast<HLSLExternalSource*>(externalSource);
  return hlsl->CheckTemplateArgumentListForHLSL(Template, TemplateLoc, TemplateArgList);
}

/// <summary>Deduces template arguments on a function call in an HLSL program.</summary>
Sema::TemplateDeductionResult hlsl::DeduceTemplateArgumentsForHLSL(Sema* self,
  FunctionTemplateDecl *FunctionTemplate,
  TemplateArgumentListInfo *ExplicitTemplateArgs, ArrayRef<Expr *> Args,
  FunctionDecl *&Specialization, TemplateDeductionInfo &Info)
{
  return HLSLExternalSource::FromSema(self)
    ->DeduceTemplateArgumentsForHLSL(FunctionTemplate, ExplicitTemplateArgs, Args, Specialization, Info);
}

void hlsl::DiagnoseControlFlowConditionForHLSL(Sema *self, Expr *condExpr, StringRef StmtName) {
  while (ImplicitCastExpr *IC = dyn_cast<ImplicitCastExpr>(condExpr)) {
    if (IC->getCastKind() == CastKind::CK_HLSLMatrixTruncationCast ||
        IC->getCastKind() == CastKind::CK_HLSLVectorTruncationCast) {
      self->Diag(condExpr->getLocStart(),
                 diag::err_hlsl_control_flow_cond_not_scalar)
          << StmtName;
      return;
    }
    condExpr = IC->getSubExpr();
  }
}

static bool ShaderModelsMatch(const StringRef& left, const StringRef& right)
{
  // TODO: handle shorthand cases.
  return left.size() == 0 || right.size() == 0 || left.equals(right);
}

void hlsl::DiagnosePackingOffset(
  clang::Sema* self,
  SourceLocation loc,
  clang::QualType type,
  int componentOffset)
{
  DXASSERT_NOMSG(0 <= componentOffset && componentOffset <= 3);

  if (componentOffset > 0) {
    HLSLExternalSource* source = HLSLExternalSource::FromSema(self);
    ArBasicKind element = source->GetTypeElementKind(type);
    ArTypeObjectKind shape = source->GetTypeObjectKind(type);

    // Only perform some simple validation for now.
    if (IsObjectKindPrimitiveAggregate(shape) && IsBasicKindNumeric(element)) {
      int count = GetElementCount(type);
      if (count > (4 - componentOffset)) {
        self->Diag(loc, diag::err_hlsl_register_or_offset_bind_not_valid);
      }
    }
  }
}

void hlsl::DiagnoseRegisterType(
  clang::Sema* self,
  clang::SourceLocation loc,
  clang::QualType type,
  char registerType)
{
  // Register type can be zero if only a register space was provided.
  if (registerType == 0)
    return;

  if (registerType >= 'A' && registerType <= 'Z')
    registerType = registerType + ('a' - 'A');

  HLSLExternalSource* source = HLSLExternalSource::FromSema(self);
  ArBasicKind element = source->GetTypeElementKind(type);
  StringRef expected("none");
  bool isValid = true;
  bool isWarning = false;
  switch (element)
  {
  case AR_BASIC_BOOL:
  case AR_BASIC_LITERAL_FLOAT:
  case AR_BASIC_FLOAT16:
  case AR_BASIC_FLOAT32_PARTIAL_PRECISION:
  case AR_BASIC_FLOAT32:
  case AR_BASIC_FLOAT64:
  case AR_BASIC_LITERAL_INT:
  case AR_BASIC_INT8:
  case AR_BASIC_UINT8:
  case AR_BASIC_INT16:
  case AR_BASIC_UINT16:
  case AR_BASIC_INT32:
  case AR_BASIC_UINT32:
  case AR_BASIC_INT64:
  case AR_BASIC_UINT64:

  case AR_BASIC_MIN10FLOAT:
  case AR_BASIC_MIN16FLOAT:
  case AR_BASIC_MIN12INT:
  case AR_BASIC_MIN16INT:
  case AR_BASIC_MIN16UINT:
    expected = "'b', 'c', or 'i'";
    isValid = registerType == 'b' || registerType == 'c' || registerType == 'i';
    break;

  case AR_OBJECT_TEXTURE1D:
  case AR_OBJECT_TEXTURE1D_ARRAY:
  case AR_OBJECT_TEXTURE2D:
  case AR_OBJECT_TEXTURE2D_ARRAY:
  case AR_OBJECT_TEXTURE3D:
  case AR_OBJECT_TEXTURECUBE:
  case AR_OBJECT_TEXTURECUBE_ARRAY:
  case AR_OBJECT_TEXTURE2DMS:
  case AR_OBJECT_TEXTURE2DMS_ARRAY:
    expected = "'t' or 's'";
    isValid = registerType == 't' || registerType == 's';
    break;

  case AR_OBJECT_SAMPLER:
  case AR_OBJECT_SAMPLER1D:
  case AR_OBJECT_SAMPLER2D:
  case AR_OBJECT_SAMPLER3D:
  case AR_OBJECT_SAMPLERCUBE:
  case AR_OBJECT_SAMPLERCOMPARISON:
    expected = "'s' or 't'";
    isValid = registerType == 's' || registerType == 't';
    break;

  case AR_OBJECT_BUFFER:
    expected = "'t'";
    isValid = registerType == 't';
    break;

  case AR_OBJECT_POINTSTREAM:
  case AR_OBJECT_LINESTREAM:
  case AR_OBJECT_TRIANGLESTREAM:
    isValid = false;
    isWarning = true;
    break;

  case AR_OBJECT_INPUTPATCH:
  case AR_OBJECT_OUTPUTPATCH:
    isValid = false;
    isWarning = true;
    break;

  case AR_OBJECT_RWTEXTURE1D:
  case AR_OBJECT_RWTEXTURE1D_ARRAY:
  case AR_OBJECT_RWTEXTURE2D:
  case AR_OBJECT_RWTEXTURE2D_ARRAY:
  case AR_OBJECT_RWTEXTURE3D:
  case AR_OBJECT_RWBUFFER:
    expected = "'u'";
    isValid = registerType == 'u';
    break;

  case AR_OBJECT_BYTEADDRESS_BUFFER:
  case AR_OBJECT_STRUCTURED_BUFFER:
    expected = "'t'";
    isValid = registerType == 't';
    break;

  case AR_OBJECT_CONSUME_STRUCTURED_BUFFER:
  case AR_OBJECT_RWBYTEADDRESS_BUFFER:
  case AR_OBJECT_RWSTRUCTURED_BUFFER:
  case AR_OBJECT_RWSTRUCTURED_BUFFER_ALLOC:
  case AR_OBJECT_RWSTRUCTURED_BUFFER_CONSUME:
  case AR_OBJECT_APPEND_STRUCTURED_BUFFER:
    expected = "'u'";
    isValid = registerType == 'u';
    break;

  case AR_OBJECT_CONSTANT_BUFFER:
    expected = "'b'";
    isValid = registerType == 'b';
    break;
  case AR_OBJECT_TEXTURE_BUFFER:
    expected = "'t'";
    isValid = registerType == 't';
    break;

  case AR_OBJECT_ROVBUFFER:
  case AR_OBJECT_ROVBYTEADDRESS_BUFFER:
  case AR_OBJECT_ROVSTRUCTURED_BUFFER:
  case AR_OBJECT_ROVTEXTURE1D:
  case AR_OBJECT_ROVTEXTURE1D_ARRAY:
  case AR_OBJECT_ROVTEXTURE2D:
  case AR_OBJECT_ROVTEXTURE2D_ARRAY:
  case AR_OBJECT_ROVTEXTURE3D:
    expected = "'u'";
    isValid = registerType == 'u';
    break;

  case AR_OBJECT_LEGACY_EFFECT:   // Used for all unsupported but ignored legacy effect types
    isWarning = true;
    break;                        // So we don't care what you tried to bind it to
  default: // Other types have no associated registers.
    break;
  }

  // fxc is inconsistent as to when it reports an error and when it ignores invalid bind semantics, so emit
  // a warning instead.
  if (!isValid) {
    unsigned DiagID = isWarning ? diag::warn_hlsl_incorrect_bind_semantic : diag::err_hlsl_incorrect_bind_semantic;
    self->Diag(loc, DiagID) << expected;
  }
}

struct NameLookup {
  FunctionDecl *Found;
  FunctionDecl *Other;
};

static NameLookup GetSingleFunctionDeclByName(clang::Sema *self, StringRef Name, bool checkPatch) {
  auto DN = DeclarationName(&self->getASTContext().Idents.get(Name));
  FunctionDecl *pFoundDecl = nullptr;
  for (auto idIter = self->IdResolver.begin(DN), idEnd = self->IdResolver.end(); idIter != idEnd; ++idIter) {
    FunctionDecl *pFnDecl = dyn_cast<FunctionDecl>(*idIter);
    if (!pFnDecl) continue;
    if (checkPatch && !self->getASTContext().IsPatchConstantFunctionDecl(pFnDecl)) continue;
    if (pFoundDecl) {
      return NameLookup{ pFoundDecl, pFnDecl };
    }
    pFoundDecl = pFnDecl;
  }
  return NameLookup{ pFoundDecl, nullptr };
}

void hlsl::DiagnoseTranslationUnit(clang::Sema *self) {
  DXASSERT_NOMSG(self != nullptr);

  // Don't bother with global validation if compilation has already failed.
  if (self->getDiagnostics().hasErrorOccurred()) {
    return;
  }
  // Don't check entry function for library.
  if (self->getLangOpts().IsHLSLLibrary) {
    // TODO: validate no recursion start from every function.
    return;
  }

  // TODO: make these error 'real' errors rather than on-the-fly things
  // Validate that the entry point is available.
  DiagnosticsEngine &Diags = self->getDiagnostics();
  FunctionDecl *pEntryPointDecl = nullptr;
  FunctionDecl *pPatchFnDecl = nullptr;
  const std::string &EntryPointName = self->getLangOpts().HLSLEntryFunction;
  if (!EntryPointName.empty()) {
    NameLookup NL = GetSingleFunctionDeclByName(self, EntryPointName, /*checkPatch*/ false);
    if (NL.Found && NL.Other) {
      // NOTE: currently we cannot hit this codepath when CodeGen is enabled, because
      // CodeGenModule::getMangledName will mangle the entry point name into the bare
      // string, and so ambiguous points will produce an error earlier on.
      unsigned id = Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
        "ambiguous entry point function");
      Diags.Report(NL.Found->getSourceRange().getBegin(), id);
      Diags.Report(NL.Other->getLocation(), diag::note_previous_definition);
      return;
    }
    pEntryPointDecl = NL.Found;
    if (!pEntryPointDecl || !pEntryPointDecl->hasBody()) {
      unsigned id = Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
        "missing entry point definition");
      Diags.Report(id);
      return;
    }
  }

  // Validate that there is no recursion; start with the entry function.
  // NOTE: the information gathered here could be used to bypass code generation
  // on functions that are unreachable (as an early form of dead code elimination).
  if (pEntryPointDecl) {
    const auto *shaderModel =
        hlsl::ShaderModel::GetByName(self->getLangOpts().HLSLProfile.c_str());

    if (shaderModel->IsGS()) {
      // Validate that GS has the maxvertexcount attribute
      if (!pEntryPointDecl->hasAttr<HLSLMaxVertexCountAttr>()) {
        self->Diag(pEntryPointDecl->getLocation(),
                   diag::err_hlsl_missing_maxvertexcount_attr);
        return;
      }
    } else if (shaderModel->IsHS()) {
      if (const HLSLPatchConstantFuncAttr *Attr =
              pEntryPointDecl->getAttr<HLSLPatchConstantFuncAttr>()) {
        NameLookup NL = GetSingleFunctionDeclByName(
            self, Attr->getFunctionName(), /*checkPatch*/ true);
        if (!NL.Found || !NL.Found->hasBody()) {
          unsigned id =
              Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
                                    "missing patch function definition");
          Diags.Report(id);
          return;
        }
        pPatchFnDecl = NL.Found;
      } else {
        self->Diag(pEntryPointDecl->getLocation(),
                   diag::err_hlsl_missing_patchconstantfunc_attr);
        return;
      }
    }

    hlsl::CallGraphWithRecurseGuard CG;
    CG.BuildForEntry(pEntryPointDecl);
    Decl *pResult = CG.CheckRecursion(pEntryPointDecl);
    if (pResult) {
      unsigned id = Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
        "recursive functions not allowed");
      Diags.Report(pResult->getSourceRange().getBegin(), id);
    }
    if (pPatchFnDecl) {
      CG.BuildForEntry(pPatchFnDecl);
      Decl *pPatchFnDecl = CG.CheckRecursion(pEntryPointDecl);
      if (pPatchFnDecl) {
        unsigned id = Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
          "recursive functions not allowed (via patch function)");
        Diags.Report(pPatchFnDecl->getSourceRange().getBegin(), id);
      }
    }
  }
}

void hlsl::DiagnoseUnusualAnnotationsForHLSL(
  Sema& S,
  std::vector<hlsl::UnusualAnnotation *>& annotations)
{
  bool packoffsetOverriddenReported = false;
  auto && iter = annotations.begin();
  auto && end = annotations.end();
  for (; iter != end; ++iter) {
    switch ((*iter)->getKind()) {
    case hlsl::UnusualAnnotation::UA_ConstantPacking: {
      hlsl::ConstantPacking* constantPacking = cast<hlsl::ConstantPacking>(*iter);

      // Check whether this will conflict with other packoffsets. If so, only issue a warning; last one wins.
      if (!packoffsetOverriddenReported) {
        auto newIter = iter;
        ++newIter;
        while (newIter != end) {
          hlsl::ConstantPacking* other = dyn_cast_or_null<hlsl::ConstantPacking>(*newIter);
          if (other != nullptr &&
            (other->Subcomponent != constantPacking->Subcomponent || other->ComponentOffset != constantPacking->ComponentOffset)) {
            S.Diag(constantPacking->Loc, diag::warn_hlsl_packoffset_overridden);
            packoffsetOverriddenReported = true;
            break;
          }
          ++newIter;
        }
      }

      break;
    }
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment* registerAssignment = cast<hlsl::RegisterAssignment>(*iter);

      // Check whether this will conflict with other register assignments of the same type.
      auto newIter = iter;
      ++newIter;
      while (newIter != end) {
        hlsl::RegisterAssignment* other = dyn_cast_or_null<hlsl::RegisterAssignment>(*newIter);

        // Same register bank and profile, but different number.
        if (other != nullptr &&
          ShaderModelsMatch(other->ShaderProfile, registerAssignment->ShaderProfile) &&
          other->RegisterType == registerAssignment->RegisterType &&
          (other->RegisterNumber != registerAssignment->RegisterNumber ||
          other->RegisterOffset != registerAssignment->RegisterOffset)) {
          // Obvious conflict - report it up front.
          S.Diag(registerAssignment->Loc, diag::err_hlsl_register_semantics_conflicting);
        }

        ++newIter;
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl: {
      // hlsl::SemanticDecl* semanticDecl = cast<hlsl::SemanticDecl>(*iter);
      // No common validation to be performed.
      break;
    }
    }
  }
}

clang::OverloadingResult
hlsl::GetBestViableFunction(clang::Sema &S, clang::SourceLocation Loc,
                      clang::OverloadCandidateSet &set,
                      clang::OverloadCandidateSet::iterator &Best) {
  return HLSLExternalSource::FromSema(&S)
      ->GetBestViableFunction(Loc, set, Best);
}

void hlsl::InitializeInitSequenceForHLSL(Sema *self,
                                         const InitializedEntity &Entity,
                                         const InitializationKind &Kind,
                                         MultiExprArg Args,
                                         bool TopLevelOfInitList,
                                         InitializationSequence *initSequence) {
  return HLSLExternalSource::FromSema(self)
    ->InitializeInitSequenceForHLSL(Entity, Kind, Args, TopLevelOfInitList, initSequence);
}

static unsigned CaculateInitListSize(HLSLExternalSource *hlslSource,
                                     const clang::InitListExpr *InitList) {
  unsigned totalSize = 0;
  for (unsigned i = 0; i < InitList->getNumInits(); i++) {
    const clang::Expr *EltInit = InitList->getInit(i);
    QualType EltInitTy = EltInit->getType();
    if (const InitListExpr *EltInitList = dyn_cast<InitListExpr>(EltInit)) {
      totalSize += CaculateInitListSize(hlslSource, EltInitList);
    } else {
      totalSize += hlslSource->GetNumBasicElements(EltInitTy);
    }
  }
  return totalSize;
}

unsigned hlsl::CaculateInitListArraySizeForHLSL(
  _In_ clang::Sema* sema,
  _In_ const clang::InitListExpr *InitList,
  _In_ const clang::QualType EltTy) {
  HLSLExternalSource *hlslSource = HLSLExternalSource::FromSema(sema);
  unsigned totalSize = CaculateInitListSize(hlslSource, InitList);
  unsigned eltSize = hlslSource->GetNumBasicElements(EltTy);

  if (totalSize > 0 && (totalSize % eltSize)==0) {
    return totalSize / eltSize;
  } else {
    return 0;
  }
}

bool hlsl::IsConversionToLessOrEqualElements(
  _In_ clang::Sema* self,
  const clang::ExprResult& sourceExpr,
  const clang::QualType& targetType,
  bool explicitConversion)
{
  return HLSLExternalSource::FromSema(self)
    ->IsConversionToLessOrEqualElements(sourceExpr, targetType, explicitConversion);
}

bool hlsl::LookupMatrixMemberExprForHLSL(
  Sema* self,
  Expr& BaseExpr,
  DeclarationName MemberName,
  bool IsArrow,
  SourceLocation OpLoc,
  SourceLocation MemberLoc,
  ExprResult* result)
{
  return HLSLExternalSource::FromSema(self)
    ->LookupMatrixMemberExprForHLSL(BaseExpr, MemberName, IsArrow, OpLoc, MemberLoc, result);
}

bool hlsl::LookupVectorMemberExprForHLSL(
  Sema* self,
  Expr& BaseExpr,
  DeclarationName MemberName,
  bool IsArrow,
  SourceLocation OpLoc,
  SourceLocation MemberLoc,
  ExprResult* result)
{
  return HLSLExternalSource::FromSema(self)
    ->LookupVectorMemberExprForHLSL(BaseExpr, MemberName, IsArrow, OpLoc, MemberLoc, result);
}

bool hlsl::LookupArrayMemberExprForHLSL(
  Sema* self,
  Expr& BaseExpr,
  DeclarationName MemberName,
  bool IsArrow,
  SourceLocation OpLoc,
  SourceLocation MemberLoc,
  ExprResult* result)
{
  return HLSLExternalSource::FromSema(self)
    ->LookupArrayMemberExprForHLSL(BaseExpr, MemberName, IsArrow, OpLoc, MemberLoc, result);
}

clang::ExprResult hlsl::MaybeConvertScalarToVector(
  _In_ clang::Sema* self,
  _In_ clang::Expr* E)
{
  return HLSLExternalSource::FromSema(self)->MaybeConvertScalarToVector(E);
}

bool hlsl::TryStaticCastForHLSL(_In_ Sema* self, ExprResult &SrcExpr,
  QualType DestType,
  Sema::CheckedConversionKind CCK,
  const SourceRange &OpRange, unsigned &msg,
  CastKind &Kind, CXXCastPath &BasePath,
  bool ListInitialization,
  bool SuppressDiagnostics,
  _Inout_opt_ StandardConversionSequence* standard)
{
  return HLSLExternalSource::FromSema(self)->TryStaticCastForHLSL(
      SrcExpr, DestType, CCK, OpRange, msg, Kind, BasePath, ListInitialization,
      SuppressDiagnostics, SuppressDiagnostics, standard);
}

clang::ExprResult hlsl::PerformHLSLConversion(
  _In_ clang::Sema* self,
  _In_ clang::Expr* From,
  _In_ clang::QualType targetType,
  _In_ const clang::StandardConversionSequence &SCS,
  _In_ clang::Sema::CheckedConversionKind CCK)
{
  return HLSLExternalSource::FromSema(self)->PerformHLSLConversion(From, targetType, SCS, CCK);
}

clang::ImplicitConversionSequence hlsl::TrySubscriptIndexInitialization(
  _In_ clang::Sema* self,
  _In_ clang::Expr* SrcExpr,
  clang::QualType DestType)
{
  return HLSLExternalSource::FromSema(self)
    ->TrySubscriptIndexInitialization(SrcExpr, DestType);
}

/// <summary>Performs HLSL-specific initialization on the specified context.</summary>
void hlsl::InitializeASTContextForHLSL(ASTContext& context)
{
  HLSLExternalSource* hlslSource = new HLSLExternalSource();
  IntrusiveRefCntPtr<ExternalASTSource> externalSource(hlslSource);
  if (hlslSource->Initialize(context)) {
    context.setExternalSource(externalSource);
  }
}

////////////////////////////////////////////////////////////////////////////////
// FlattenedTypeIterator implementation                                       //

/// <summary>Constructs a FlattenedTypeIterator for the specified type.</summary>
FlattenedTypeIterator::FlattenedTypeIterator(SourceLocation loc, QualType type, HLSLExternalSource& source) :
  m_source(source), m_draining(false), m_springLoaded(false), m_incompleteCount(0), m_typeDepth(0), m_loc(loc)
{
  if (pushTrackerForType(type, nullptr)) {
    while (!m_typeTrackers.empty() && !considerLeaf())
      consumeLeaf();
  }
}

/// <summary>Constructs a FlattenedTypeIterator for the specified expressions.</summary>
FlattenedTypeIterator::FlattenedTypeIterator(SourceLocation loc, MultiExprArg args, HLSLExternalSource& source) :
  m_source(source), m_draining(false), m_springLoaded(false), m_incompleteCount(0), m_typeDepth(0), m_loc(loc)
{
  if (!args.empty()) {
    MultiExprArg::iterator ii = args.begin();
    MultiExprArg::iterator ie = args.end();
    DXASSERT(ii != ie, "otherwise empty() returned an incorrect value");
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(ii, ie));

    if (!considerLeaf()) {
      m_typeTrackers.clear();
    }
  }
}

/// <summary>Gets the current element in the flattened type hierarchy.</summary>
QualType FlattenedTypeIterator::getCurrentElement() const
{
  return m_typeTrackers.back().Type;
}

/// <summary>Get the number of repeated current elements.</summary>
unsigned int FlattenedTypeIterator::getCurrentElementSize() const
{
  const FlattenedTypeTracker& back = m_typeTrackers.back();
  return (back.IterKind == FK_IncompleteArray) ? 1 : back.Count;
}

/// <summary>Checks whether the iterator has a current element type to report.</summary>
bool FlattenedTypeIterator::hasCurrentElement() const
{
  return m_typeTrackers.size() > 0;
}

/// <summary>Consumes count elements on this iterator.</summary>
void FlattenedTypeIterator::advanceCurrentElement(unsigned int count)
{
  DXASSERT(!m_typeTrackers.empty(), "otherwise caller should not be trying to advance to another element");
  DXASSERT(m_typeTrackers.back().IterKind == FK_IncompleteArray || count <= m_typeTrackers.back().Count, "caller should never exceed currently pending element count");

  FlattenedTypeTracker& tracker = m_typeTrackers.back();
  if (tracker.IterKind == FK_IncompleteArray)
  {
    tracker.Count += count;
    m_springLoaded = true;
  }
  else
  {
    tracker.Count -= count;
    m_springLoaded = false;
    if (m_typeTrackers.back().Count == 0)
  {
    advanceLeafTracker();
  }
}
}

unsigned int FlattenedTypeIterator::countRemaining()
{
  m_draining = true; // when draining the iterator, incomplete arrays stop functioning as an infinite array
  size_t result = 0;
  while (hasCurrentElement() && !m_springLoaded)
  {
    size_t pending = getCurrentElementSize();
    result += pending;
    advanceCurrentElement(pending);
  }
  return result;
}

void FlattenedTypeIterator::advanceLeafTracker()
{
  DXASSERT(!m_typeTrackers.empty(), "otherwise caller should not be trying to advance to another element");
  for (;;)
  {
    consumeLeaf();
    if (m_typeTrackers.empty()) {
      return;
    }

    if (considerLeaf()) {
      return;
    }
  }
}

bool FlattenedTypeIterator::considerLeaf()
{
  if (m_typeTrackers.empty()) {
    return false;
  }

  m_typeDepth++;
  if (m_typeDepth > MaxTypeDepth) {
    m_source.ReportUnsupportedTypeNesting(m_loc, m_firstType);
    m_typeTrackers.clear();
    m_typeDepth--;
    return false;
  }

  bool result = false;
  FlattenedTypeTracker& tracker = m_typeTrackers.back();
  tracker.IsConsidered = true;

  switch (tracker.IterKind) {
  case FlattenedIterKind::FK_Expressions:
    if (pushTrackerForExpression(tracker.CurrentExpr)) {
      result = considerLeaf();
    }
    break;
  case FlattenedIterKind::FK_Fields:
    if (pushTrackerForType(tracker.CurrentField->getType(), nullptr)) {
      result = considerLeaf();
    }
    break;
  case FlattenedIterKind::FK_Bases:
    if (pushTrackerForType(tracker.CurrentBase->getType(), nullptr)) {
      result = considerLeaf();
    }
    break;
  case FlattenedIterKind::FK_IncompleteArray:
    m_springLoaded = true; // fall through.
  default:
  case FlattenedIterKind::FK_Simple: {
    ArTypeObjectKind objectKind = m_source.GetTypeObjectKind(tracker.Type);
    if (objectKind != ArTypeObjectKind::AR_TOBJ_BASIC &&
        objectKind != ArTypeObjectKind::AR_TOBJ_OBJECT &&
        objectKind != ArTypeObjectKind::AR_TOBJ_STRING) {
      if (pushTrackerForType(tracker.Type, tracker.CurrentExpr)) {
        result = considerLeaf();
      }
    } else {
      result = true;
    }
  }
  }

  m_typeDepth--;
  return result;
}

void FlattenedTypeIterator::consumeLeaf()
{
  bool topConsumed = true; // Tracks whether we're processing the topmost item which we should consume.
  for (;;) {
    if (m_typeTrackers.empty()) {
      return;
    }

    FlattenedTypeTracker& tracker = m_typeTrackers.back();
    // Reach a leaf which is not considered before.
    // Stop here.
    if (!tracker.IsConsidered) {
      break;
    }
    switch (tracker.IterKind) {
    case FlattenedIterKind::FK_Expressions:
      ++tracker.CurrentExpr;
      if (tracker.CurrentExpr == tracker.EndExpr) {
        m_typeTrackers.pop_back();
        topConsumed = false;
      } else {
        return;
      }
      break;
    case FlattenedIterKind::FK_Fields:

      ++tracker.CurrentField;
      if (tracker.CurrentField == tracker.EndField) {
        m_typeTrackers.pop_back();
        topConsumed = false;
      } else {
        return;
      }
      break;
    case FlattenedIterKind::FK_Bases:
      ++tracker.CurrentBase;
      if (tracker.CurrentBase == tracker.EndBase) {
        m_typeTrackers.pop_back();
        topConsumed = false;
      } else {
        return;
      }
      break;
    case FlattenedIterKind::FK_IncompleteArray:
      if (m_draining) {
        DXASSERT(m_typeTrackers.size() == 1, "m_typeTrackers.size() == 1, otherwise incomplete array isn't topmost");
        m_incompleteCount = tracker.Count;
        m_typeTrackers.pop_back();
      }
      return;
    default:
    case FlattenedIterKind::FK_Simple: {
      m_springLoaded = false;
      if (!topConsumed) {
        DXASSERT(tracker.Count > 0, "tracker.Count > 0 - otherwise we shouldn't be on stack");
        --tracker.Count;
      }
      else {
        topConsumed = false;
      }
      if (tracker.Count == 0) {
        m_typeTrackers.pop_back();
      } else {
        return;
      }
    }
    }
  }
}

bool FlattenedTypeIterator::pushTrackerForExpression(MultiExprArg::iterator expression)
{
  Expr* e = *expression;
  Stmt::StmtClass expressionClass = e->getStmtClass();
  if (expressionClass == Stmt::StmtClass::InitListExprClass) {
    InitListExpr* initExpr = dyn_cast<InitListExpr>(e);
    if (initExpr->getNumInits() == 0) {
      return false;
    }

    MultiExprArg inits(initExpr->getInits(), initExpr->getNumInits());
    MultiExprArg::iterator ii = inits.begin();
    MultiExprArg::iterator ie = inits.end();
    DXASSERT(ii != ie, "otherwise getNumInits() returned an incorrect value");
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(ii, ie));
    return true;
  }

  return pushTrackerForType(e->getType(), expression);
}

// TODO: improve this to provide a 'peek' at intermediate types,
// which should help compare struct foo[1000] to avoid 1000 steps + per-field steps
bool FlattenedTypeIterator::pushTrackerForType(QualType type, MultiExprArg::iterator expression)
  {
  if (type->isVoidType()) {
    return false;
  }

  if (type->isFunctionType()) {
    return false;
  }

  if (m_firstType.isNull()) {
    m_firstType = type;
  }

  ArTypeObjectKind objectKind = m_source.GetTypeObjectKind(type);
  QualType elementType;
  unsigned int elementCount;
  const RecordType* recordType;
  RecordDecl::field_iterator fi, fe;
  switch (objectKind)
  {
  case ArTypeObjectKind::AR_TOBJ_ARRAY:
    // TODO: handle multi-dimensional arrays
    elementType = type->getAsArrayTypeUnsafe()->getElementType(); // handle arrays of arrays
    elementCount = GetArraySize(type);
    if (elementCount == 0) {
      if (type->isIncompleteArrayType()) {
        m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(elementType));
        return true;
      }
      return false;
    }

    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(
      elementType, elementCount, nullptr));

    return true;
  case ArTypeObjectKind::AR_TOBJ_BASIC:
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(type, 1, expression));
    return true;
  case ArTypeObjectKind::AR_TOBJ_COMPOUND: {
    recordType = type->getAsStructureType();
    if (recordType == nullptr)
      recordType = dyn_cast<RecordType>(type.getTypePtr());

    fi = recordType->getDecl()->field_begin();
    fe = recordType->getDecl()->field_end();

    bool bAddTracker = false;

    // Skip empty struct.
    if (fi != fe) {
      m_typeTrackers.push_back(
          FlattenedTypeIterator::FlattenedTypeTracker(type, fi, fe));
      type = (*fi)->getType();
      bAddTracker = true;
    }

    if (CXXRecordDecl *cxxRecordDecl =
            dyn_cast<CXXRecordDecl>(recordType->getDecl())) {
      // We'll error elsewhere if the record has no definition,
      // just don't attempt to use it.
      if (cxxRecordDecl->hasDefinition()) {
        CXXRecordDecl::base_class_iterator bi, be;
        bi = cxxRecordDecl->bases_begin();
        be = cxxRecordDecl->bases_end();
        if (bi != be) {
          // Add type tracker for base.
          // Add base after child to make sure base considered first.
          m_typeTrackers.push_back(
            FlattenedTypeIterator::FlattenedTypeTracker(type, bi, be));
          bAddTracker = true;
        }
      }
    }
    return bAddTracker;
  }
  case ArTypeObjectKind::AR_TOBJ_MATRIX:
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(
      m_source.GetMatrixOrVectorElementType(type),
      GetElementCount(type), nullptr));
    return true;
  case ArTypeObjectKind::AR_TOBJ_VECTOR:
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(
      m_source.GetMatrixOrVectorElementType(type),
      GetHLSLVecSize(type), nullptr));
    return true;
  case ArTypeObjectKind::AR_TOBJ_OBJECT: {
    if (m_source.IsSubobjectType(type)) {
      // subobjects are initialized with initialization lists
      recordType = type->getAsStructureType();
      fi = recordType->getDecl()->field_begin();
      fe = recordType->getDecl()->field_end();

      m_typeTrackers.push_back(
          FlattenedTypeIterator::FlattenedTypeTracker(type, fi, fe));
      return true;
    }
    else {
      // Object have no sub-types.
      m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(
        type.getCanonicalType(), 1, expression));
      return true;
    }
  }
  case ArTypeObjectKind::AR_TOBJ_STRING: {
    // Strings have no sub-types.
    m_typeTrackers.push_back(FlattenedTypeIterator::FlattenedTypeTracker(
      type.getCanonicalType(), 1, expression));
    return true;
  }
  default:
    DXASSERT(false, "unreachable");
    return false;
  }
}

FlattenedTypeIterator::ComparisonResult
FlattenedTypeIterator::CompareIterators(
  HLSLExternalSource& source,
  SourceLocation loc,
  FlattenedTypeIterator& leftIter,
  FlattenedTypeIterator& rightIter)
{
  FlattenedTypeIterator::ComparisonResult result;
  result.LeftCount = 0;
  result.RightCount = 0;
  result.AreElementsEqual = true; // Until proven otherwise.
  result.CanConvertElements = true; // Until proven otherwise.

  while (leftIter.hasCurrentElement() && rightIter.hasCurrentElement())
  {
    Expr* actualExpr = rightIter.getExprOrNull();
    bool hasExpr = actualExpr != nullptr;
    StmtExpr scratchExpr(nullptr, rightIter.getCurrentElement(), NoLoc, NoLoc);
    StandardConversionSequence standard;
    ExprResult convertedExpr;
    if (!source.CanConvert(loc,
                           hasExpr ? actualExpr : &scratchExpr, 
                           leftIter.getCurrentElement(), 
                           ExplicitConversionFalse, 
                           nullptr,
                           &standard)) {
      result.AreElementsEqual = false;
      result.CanConvertElements = false;
      break;
    }
    else if (hasExpr && (standard.First != ICK_Identity || !standard.isIdentityConversion()))
    {
      convertedExpr = source.getSema()->PerformImplicitConversion(actualExpr, 
                                                   leftIter.getCurrentElement(), 
                                                   standard,
                                                   Sema::AA_Casting, 
                                                   Sema::CCK_ImplicitConversion);
    }

    if (rightIter.getCurrentElement()->getCanonicalTypeUnqualified() !=
        leftIter.getCurrentElement()->getCanonicalTypeUnqualified())
    {
      result.AreElementsEqual = false;
    }

    unsigned int advance = std::min(leftIter.getCurrentElementSize(), rightIter.getCurrentElementSize());
    DXASSERT(advance > 0, "otherwise one iterator should report empty");

    // If we need to apply conversions to the expressions, then advance a single element.
    if (hasExpr && convertedExpr.isUsable()) {
      rightIter.replaceExpr(convertedExpr.get());
      advance = 1;
    }

    leftIter.advanceCurrentElement(advance);
    rightIter.advanceCurrentElement(advance);
    result.LeftCount += advance;
    result.RightCount += advance;
  }

  result.LeftCount += leftIter.countRemaining();
  result.RightCount += rightIter.countRemaining();

  return result;
}

FlattenedTypeIterator::ComparisonResult
FlattenedTypeIterator::CompareTypes(
  HLSLExternalSource& source,
  SourceLocation leftLoc, SourceLocation rightLoc,
  QualType left, QualType right)
{
  FlattenedTypeIterator leftIter(leftLoc, left, source);
  FlattenedTypeIterator rightIter(rightLoc, right, source);

  return CompareIterators(source, leftLoc, leftIter, rightIter);
}

FlattenedTypeIterator::ComparisonResult
FlattenedTypeIterator::CompareTypesForInit(
  HLSLExternalSource& source, QualType left, MultiExprArg args,
  SourceLocation leftLoc, SourceLocation rightLoc)
{
  FlattenedTypeIterator leftIter(leftLoc, left, source);
  FlattenedTypeIterator rightIter(rightLoc, args, source);

  return CompareIterators(source, leftLoc, leftIter, rightIter);
}

////////////////////////////////////////////////////////////////////////////////
// Attribute processing support.                                              //

static int ValidateAttributeIntArg(Sema& S, const AttributeList &Attr, unsigned index = 0)
{
  int64_t value = 0;

  if (Attr.getNumArgs() > index)
  {
    Expr *E = nullptr;
    if (!Attr.isArgExpr(index)) {
      // For case arg is constant variable.
      IdentifierLoc *loc = Attr.getArgAsIdent(index);

      VarDecl *decl = dyn_cast_or_null<VarDecl>(
          S.LookupSingleName(S.getCurScope(), loc->Ident, loc->Loc,
                             Sema::LookupNameKind::LookupOrdinaryName));
      if (!decl) {
        S.Diag(Attr.getLoc(), diag::warn_hlsl_attribute_expects_uint_literal) << Attr.getName();
        return value;
      }
      Expr *init = decl->getInit();
      if (!init) {
        S.Diag(Attr.getLoc(), diag::warn_hlsl_attribute_expects_uint_literal) << Attr.getName();
        return value;
      }
      E = init;
    } else
      E = Attr.getArgAsExpr(index);

    clang::APValue ArgNum;
    bool displayError = false;
    if (E->isTypeDependent() || E->isValueDependent() || !E->isCXX11ConstantExpr(S.Context, &ArgNum))
    {
      displayError = true;
    }
    else
    {
      if (ArgNum.isInt())
      {
        value = ArgNum.getInt().getSExtValue();
      }
      else if (ArgNum.isFloat())
      {
        llvm::APSInt floatInt;
        bool isPrecise;
        if (ArgNum.getFloat().convertToInteger(floatInt, llvm::APFloat::rmTowardZero, &isPrecise) == llvm::APFloat::opStatus::opOK)
        {
          value = floatInt.getSExtValue();
        }
        else
        {
          S.Diag(Attr.getLoc(), diag::warn_hlsl_attribute_expects_uint_literal) << Attr.getName();
        }
      }
      else
      {
        displayError = true;
      }

      if (value < 0)
      {
        S.Diag(Attr.getLoc(), diag::warn_hlsl_attribute_expects_uint_literal) << Attr.getName();
      }
    }

    if (displayError)
    {
      S.Diag(Attr.getLoc(), diag::err_attribute_argument_type)
        << Attr.getName() << AANT_ArgumentIntegerConstant
        << E->getSourceRange();
    }
  }

  return (int)value;
}

// TODO: support float arg directly.
static int ValidateAttributeFloatArg(Sema &S, const AttributeList &Attr,
                                           unsigned index = 0) {
  int value = 0;
  if (Attr.getNumArgs() > index) {
    Expr *E = Attr.getArgAsExpr(index);

    if (FloatingLiteral *FL = dyn_cast<FloatingLiteral>(E)) {
      llvm::APFloat flV = FL->getValue();
      if (flV.getSizeInBits(flV.getSemantics()) == 64) {
        llvm::APInt intV = llvm::APInt::floatToBits(flV.convertToDouble());
        value = intV.getLimitedValue();
      } else {
        llvm::APInt intV = llvm::APInt::floatToBits(flV.convertToFloat());
        value = intV.getLimitedValue();
      }
    } else if (IntegerLiteral *IL = dyn_cast<IntegerLiteral>(E)) {
      llvm::APInt intV =
          llvm::APInt::floatToBits((float)IL->getValue().getLimitedValue());
      value = intV.getLimitedValue();
    } else {
      S.Diag(E->getLocStart(), diag::err_hlsl_attribute_expects_float_literal)
          << Attr.getName();
    }
  }
  return value;
}

static Stmt* IgnoreParensAndDecay(Stmt* S)
{
  for (;;)
  {
    switch (S->getStmtClass())
    {
    case Stmt::ParenExprClass:
      S = cast<ParenExpr>(S)->getSubExpr();
      break;
    case Stmt::ImplicitCastExprClass:
      {
        ImplicitCastExpr* castExpr = cast<ImplicitCastExpr>(S);
        if (castExpr->getCastKind() != CK_ArrayToPointerDecay &&
            castExpr->getCastKind() != CK_NoOp &&
            castExpr->getCastKind() != CK_LValueToRValue)
        {
          return S;
        }
        S = castExpr->getSubExpr();
      }
      break;
    default:
      return S;
    }
  }
}

static Expr* ValidateClipPlaneArraySubscriptExpr(Sema& S, ArraySubscriptExpr* E)
{
  DXASSERT_NOMSG(E != nullptr);

  Expr* subscriptExpr = E->getIdx();
  subscriptExpr = dyn_cast<Expr>(subscriptExpr->IgnoreParens());
  if (subscriptExpr == nullptr ||
      subscriptExpr->isTypeDependent() || subscriptExpr->isValueDependent() ||
      !subscriptExpr->isCXX11ConstantExpr(S.Context))
  {
    S.Diag(
      (subscriptExpr == nullptr) ? E->getLocStart() : subscriptExpr->getLocStart(),
      diag::err_hlsl_unsupported_clipplane_argument_subscript_expression);
    return nullptr;
  }

  return E->getBase();
}

static bool IsValidClipPlaneDecl(Decl* D)
{
  Decl::Kind kind = D->getKind();
  if (kind == Decl::Var)
  {
    VarDecl* varDecl = cast<VarDecl>(D);
    if (varDecl->getStorageClass() == StorageClass::SC_Static &&
        varDecl->getType().isConstQualified())
    {
      return false;
    }

    return true;
  }
  else if (kind == Decl::Field)
  {
    return true;
  }
  return false;
}

static Expr* ValidateClipPlaneExpr(Sema& S, Expr* E)
{
  Stmt* cursor = E;

  // clip plane expressions are a linear path, so no need to traverse the tree here.
  while (cursor != nullptr)
  {
    bool supported = true;
    cursor = IgnoreParensAndDecay(cursor);
    switch (cursor->getStmtClass())
    {
    case Stmt::ArraySubscriptExprClass:
      cursor = ValidateClipPlaneArraySubscriptExpr(S, cast<ArraySubscriptExpr>(cursor));
      if (cursor == nullptr)
      {
        // nullptr indicates failure, and the error message has already been printed out
        return nullptr;
      }
      break;
    case Stmt::DeclRefExprClass:
      {
        DeclRefExpr* declRef = cast<DeclRefExpr>(cursor);
        Decl* decl = declRef->getDecl();
        supported = IsValidClipPlaneDecl(decl);
        cursor = supported ? nullptr : cursor;
      }
      break;
    case Stmt::MemberExprClass:
      {
        MemberExpr* member = cast<MemberExpr>(cursor);
        supported = IsValidClipPlaneDecl(member->getMemberDecl());
        cursor = supported ? member->getBase() : cursor;
      }
      break;
    default:
      supported = false;
      break;
    }

    if (!supported)
    {
      DXASSERT(cursor != nullptr, "otherwise it was cleared when the supported flag was set to false");
      S.Diag(cursor->getLocStart(), diag::err_hlsl_unsupported_clipplane_argument_expression);
      return nullptr;
    }
  }

  // Validate that the type is a float4.
  QualType expressionType = E->getType();
  HLSLExternalSource* hlslSource = HLSLExternalSource::FromSema(&S);
  if (hlslSource->GetTypeElementKind(expressionType) != ArBasicKind::AR_BASIC_FLOAT32 ||
      hlslSource->GetTypeObjectKind(expressionType) != ArTypeObjectKind::AR_TOBJ_VECTOR)
  {
    S.Diag(E->getLocStart(), diag::err_hlsl_unsupported_clipplane_argument_type) << expressionType;
    return nullptr;
  }

  return E;
}

static Attr* HandleClipPlanes(Sema& S, const AttributeList &A)
{
  Expr* clipExprs[6];
  for (unsigned int index = 0; index < _countof(clipExprs); index++)
  {
    if (A.getNumArgs() <= index)
    {
      clipExprs[index] = nullptr;
      continue;
    }

    Expr *E = A.getArgAsExpr(index);
    clipExprs[index] = ValidateClipPlaneExpr(S, E);
  }

  return ::new (S.Context) HLSLClipPlanesAttr(A.getRange(), S.Context,
    clipExprs[0], clipExprs[1], clipExprs[2], clipExprs[3], clipExprs[4], clipExprs[5],
    A.getAttributeSpellingListIndex());
}

static Attr* HandleUnrollAttribute(Sema& S, const AttributeList &Attr)
{
  int argValue = ValidateAttributeIntArg(S, Attr);
  // Default value is 0 (full unroll).
  if (Attr.getNumArgs() == 0) argValue = 0;
  return ::new (S.Context) HLSLUnrollAttr(Attr.getRange(), S.Context,
    argValue, Attr.getAttributeSpellingListIndex());
}

static void ValidateAttributeOnLoop(Sema& S, Stmt* St, const AttributeList &Attr)
{
  Stmt::StmtClass stClass = St->getStmtClass();
  if (stClass != Stmt::ForStmtClass && stClass != Stmt::WhileStmtClass && stClass != Stmt::DoStmtClass)
  {
    S.Diag(Attr.getLoc(), diag::warn_hlsl_unsupported_statement_for_loop_attribute)
      << Attr.getName();
  }
}

static void ValidateAttributeOnSwitch(Sema& S, Stmt* St, const AttributeList &Attr)
{
  Stmt::StmtClass stClass = St->getStmtClass();
  if (stClass != Stmt::SwitchStmtClass)
  {
    S.Diag(Attr.getLoc(), diag::warn_hlsl_unsupported_statement_for_switch_attribute)
      << Attr.getName();
  }
}

static void ValidateAttributeOnSwitchOrIf(Sema& S, Stmt* St, const AttributeList &Attr)
{
  Stmt::StmtClass stClass = St->getStmtClass();
  if (stClass != Stmt::SwitchStmtClass && stClass != Stmt::IfStmtClass)
  {
    S.Diag(Attr.getLoc(), diag::warn_hlsl_unsupported_statement_for_if_switch_attribute)
      << Attr.getName();
  }
}

static StringRef ValidateAttributeStringArg(Sema& S, const AttributeList &A, _In_opt_z_ const char* values, unsigned index = 0)
{
  // values is an optional comma-separated list of potential values.
  if (A.getNumArgs() <= index)
    return StringRef();

  Expr* E = A.getArgAsExpr(index);
  if (E->isTypeDependent() || E->isValueDependent() || E->getStmtClass() != Stmt::StringLiteralClass)
  {
    S.Diag(E->getLocStart(), diag::err_hlsl_attribute_expects_string_literal)
      << A.getName();
    return StringRef();
  }

  StringLiteral* sl = cast<StringLiteral>(E);
  StringRef result = sl->getString();

  // Return result with no additional validation.
  if (values == nullptr)
  {
    return result;
  }

  const char* value = values;
  while (*value != '\0')
  {
    DXASSERT_NOMSG(*value != ','); // no leading commas in values

    // Look for a match.
    const char* argData = result.data();
    size_t argDataLen = result.size();

    while (argDataLen != 0 && *argData == *value && *value)
    {
      ++argData;
      ++value;
      --argDataLen;
    }

    // Match found if every input character matched.
    if (argDataLen == 0 && (*value == '\0' || *value == ','))
    {
      return result;
    }

    // Move to next separator.
    while (*value != '\0' && *value != ',')
    {
      ++value;
    }

    // Move to the start of the next item if any.
    if (*value == ',') value++;
  }

  DXASSERT_NOMSG(*value == '\0'); // no other terminating conditions

  // No match found.
  S.Diag(E->getLocStart(), diag::err_hlsl_attribute_expects_string_literal_from_list)
    << A.getName() << values;
  return StringRef();
}

static
bool ValidateAttributeTargetIsFunction(Sema& S, Decl* D, const AttributeList &A)
{
  if (D->isFunctionOrFunctionTemplate())
  {
    return true;
  }

  S.Diag(A.getLoc(), diag::err_hlsl_attribute_valid_on_function_only);
  return false;
}

void hlsl::HandleDeclAttributeForHLSL(Sema &S, Decl *D, const AttributeList &A, bool& Handled)
{
  DXASSERT_NOMSG(D != nullptr);
  DXASSERT_NOMSG(!A.isInvalid());

  Attr* declAttr = nullptr;
  Handled = true;
  switch (A.getKind())
  {
  case AttributeList::AT_HLSLIn:
    declAttr = ::new (S.Context) HLSLInAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLOut:
    declAttr = ::new (S.Context) HLSLOutAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLInOut:
    declAttr = ::new (S.Context) HLSLInOutAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;

  case AttributeList::AT_HLSLNoInterpolation:
    declAttr = ::new (S.Context) HLSLNoInterpolationAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLLinear:
  case AttributeList::AT_HLSLCenter:
    declAttr = ::new (S.Context) HLSLLinearAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLNoPerspective:
    declAttr = ::new (S.Context) HLSLNoPerspectiveAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLSample:
    declAttr = ::new (S.Context) HLSLSampleAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLCentroid:
    declAttr = ::new (S.Context) HLSLCentroidAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;

  case AttributeList::AT_HLSLPrecise:
    declAttr = ::new (S.Context) HLSLPreciseAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLShared:
    declAttr = ::new (S.Context) HLSLSharedAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLGroupShared:
    declAttr = ::new (S.Context) HLSLGroupSharedAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      VD->setType(S.Context.getAddrSpaceQualType(VD->getType(), DXIL::kTGSMAddrSpace));
    }
    break;
  case AttributeList::AT_HLSLUniform:
    declAttr = ::new (S.Context) HLSLUniformAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;

  case AttributeList::AT_HLSLColumnMajor:
    declAttr = ::new (S.Context) HLSLColumnMajorAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLRowMajor:
    declAttr = ::new (S.Context) HLSLRowMajorAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;

  case AttributeList::AT_HLSLUnorm:
    declAttr = ::new (S.Context) HLSLUnormAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLSnorm:
    declAttr = ::new (S.Context) HLSLSnormAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;

  case AttributeList::AT_HLSLPoint:
    declAttr = ::new (S.Context) HLSLPointAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLLine:
    declAttr = ::new (S.Context) HLSLLineAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLLineAdj:
    declAttr = ::new (S.Context) HLSLLineAdjAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLTriangle:
    declAttr = ::new (S.Context) HLSLTriangleAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLTriangleAdj:
    declAttr = ::new (S.Context) HLSLTriangleAdjAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLGloballyCoherent:
    declAttr = ::new (S.Context) HLSLGloballyCoherentAttr(
        A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;

  default:
    Handled = false;
    break;
  }

  if (declAttr != nullptr)
  {
    DXASSERT_NOMSG(Handled);
    D->addAttr(declAttr);
    return;
  }

  Handled = true;
  switch (A.getKind())
  {
  // These apply to statements, not declarations. The warning messages clarify this properly.
  case AttributeList::AT_HLSLUnroll:
  case AttributeList::AT_HLSLAllowUAVCondition:
  case AttributeList::AT_HLSLLoop:
  case AttributeList::AT_HLSLFastOpt:
    S.Diag(A.getLoc(), diag::warn_hlsl_unsupported_statement_for_loop_attribute)
      << A.getName();
    return;
  case AttributeList::AT_HLSLBranch:
  case AttributeList::AT_HLSLFlatten:
    S.Diag(A.getLoc(), diag::warn_hlsl_unsupported_statement_for_if_switch_attribute)
      << A.getName();
    return;
  case AttributeList::AT_HLSLForceCase:
  case AttributeList::AT_HLSLCall:
    S.Diag(A.getLoc(), diag::warn_hlsl_unsupported_statement_for_switch_attribute)
      << A.getName();
    return;

  // These are the cases that actually apply to declarations.
  case AttributeList::AT_HLSLClipPlanes:
    declAttr = HandleClipPlanes(S, A);
    break;
  case AttributeList::AT_HLSLDomain:
    declAttr = ::new (S.Context) HLSLDomainAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, "tri,quad,isoline"), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLEarlyDepthStencil:
    declAttr = ::new (S.Context) HLSLEarlyDepthStencilAttr(A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLInstance:
    declAttr = ::new (S.Context) HLSLInstanceAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLMaxTessFactor:
    declAttr = ::new (S.Context) HLSLMaxTessFactorAttr(A.getRange(), S.Context,
      ValidateAttributeFloatArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLNumThreads:
    declAttr = ::new (S.Context) HLSLNumThreadsAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), ValidateAttributeIntArg(S, A, 1), ValidateAttributeIntArg(S, A, 2),
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLRootSignature:
    declAttr = ::new (S.Context) HLSLRootSignatureAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, /*validate strings*/nullptr),
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLOutputControlPoints:
    declAttr = ::new (S.Context) HLSLOutputControlPointsAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLOutputTopology:
    declAttr = ::new (S.Context) HLSLOutputTopologyAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, "point,line,triangle,triangle_cw,triangle_ccw"), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLPartitioning:
    declAttr = ::new (S.Context) HLSLPartitioningAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, "integer,fractional_even,fractional_odd,pow2"), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLPatchConstantFunc:
    declAttr = ::new (S.Context) HLSLPatchConstantFuncAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, nullptr), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLShader:
    declAttr = ::new (S.Context) HLSLShaderAttr(
        A.getRange(), S.Context,
        ValidateAttributeStringArg(S, A,
                                   "compute,vertex,pixel,hull,domain,geometry,raygeneration,intersection,anyhit,closesthit,miss,callable"),
        A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLMaxVertexCount:
    declAttr = ::new (S.Context) HLSLMaxVertexCountAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLExperimental:
    declAttr = ::new (S.Context) HLSLExperimentalAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, nullptr, 0), ValidateAttributeStringArg(S, A, nullptr, 1),
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_NoInline:
    declAttr = ::new (S.Context) NoInlineAttr(A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLExport:
    declAttr = ::new (S.Context) HLSLExportAttr(A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  default:
    Handled = false;
    break;  // SPIRV Change: was return;
  }

  if (declAttr != nullptr)
  {
    DXASSERT_NOMSG(Handled);
    D->addAttr(declAttr);

    // The attribute has been set but will have no effect. Validation will emit a diagnostic
    // and prevent code generation.
    ValidateAttributeTargetIsFunction(S, D, A);

    return; // SPIRV Change
  }

  // SPIRV Change Starts
  Handled = true;
  switch (A.getKind())
  {
  case AttributeList::AT_VKBuiltIn:
    declAttr = ::new (S.Context) VKBuiltInAttr(A.getRange(), S.Context,
      ValidateAttributeStringArg(S, A, "PointSize,HelperInvocation,BaseVertex,BaseInstance,DrawIndex,DeviceIndex"),
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKLocation:
    declAttr = ::new (S.Context) VKLocationAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKIndex:
    declAttr = ::new (S.Context) VKIndexAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKBinding:
    declAttr = ::new (S.Context) VKBindingAttr(
        A.getRange(), S.Context, ValidateAttributeIntArg(S, A),
        A.getNumArgs() < 2 ? INT_MIN : ValidateAttributeIntArg(S, A, 1),
        A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKCounterBinding:
    declAttr = ::new (S.Context) VKCounterBindingAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKPushConstant:
    declAttr = ::new (S.Context) VKPushConstantAttr(A.getRange(), S.Context,
      A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKOffset:
    declAttr = ::new (S.Context) VKOffsetAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKInputAttachmentIndex:
    declAttr = ::new (S.Context) VKInputAttachmentIndexAttr(
        A.getRange(), S.Context, ValidateAttributeIntArg(S, A),
        A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKConstantId:
    declAttr = ::new (S.Context) VKConstantIdAttr(A.getRange(), S.Context,
      ValidateAttributeIntArg(S, A), A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKPostDepthCoverage:
    declAttr = ::new (S.Context) VKPostDepthCoverageAttr(A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_VKShaderRecordNV:
    declAttr = ::new (S.Context) VKShaderRecordNVAttr(A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  default:
    Handled = false;
    return;
  }

  if (declAttr != nullptr)
  {
    DXASSERT_NOMSG(Handled);
    D->addAttr(declAttr);
  }
  // SPIRV Change Ends
}

/// <summary>Processes an attribute for a statement.</summary>
/// <param name="S">Sema with context.</param>
/// <param name="St">Statement annotated.</param>
/// <param name="A">Single parsed attribute to process.</param>
/// <param name="Range">Range of all attribute lists (useful for FixIts to suggest inclusions).</param>
/// <param name="Handled">After execution, whether this was recognized and handled.</param>
/// <returns>An attribute instance if processed, nullptr if not recognized or an error was found.</returns>
Attr *hlsl::ProcessStmtAttributeForHLSL(Sema &S, Stmt *St, const AttributeList &A, SourceRange Range, bool& Handled)
{
  // | Construct        | Allowed Attributes                         |
  // +------------------+--------------------------------------------+
  // | for, while, do   | loop, fastopt, unroll, allow_uav_condition |
  // | if               | branch, flatten                            |
  // | switch           | branch, flatten, forcecase, call           |

  Attr * result = nullptr;
  Handled = true;

  switch (A.getKind())
  {
  case AttributeList::AT_HLSLUnroll:
    ValidateAttributeOnLoop(S, St, A);
    result = HandleUnrollAttribute(S, A);
    break;
  case AttributeList::AT_HLSLAllowUAVCondition:
    ValidateAttributeOnLoop(S, St, A);
    result = ::new (S.Context) HLSLAllowUAVConditionAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLLoop:
    ValidateAttributeOnLoop(S, St, A);
    result = ::new (S.Context) HLSLLoopAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLFastOpt:
    ValidateAttributeOnLoop(S, St, A);
    result = ::new (S.Context) HLSLFastOptAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLBranch:
    ValidateAttributeOnSwitchOrIf(S, St, A);
    result = ::new (S.Context) HLSLBranchAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLFlatten:
    ValidateAttributeOnSwitchOrIf(S, St, A);
    result = ::new (S.Context) HLSLFlattenAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLForceCase:
    ValidateAttributeOnSwitch(S, St, A);
    result = ::new (S.Context) HLSLForceCaseAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  case AttributeList::AT_HLSLCall:
    ValidateAttributeOnSwitch(S, St, A);
    result = ::new (S.Context) HLSLCallAttr(
      A.getRange(), S.Context, A.getAttributeSpellingListIndex());
    break;
  default:
    Handled = false;
    break;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of Sema members.                                            //

Decl* Sema::ActOnStartHLSLBuffer(
  Scope* bufferScope,
  bool cbuffer, SourceLocation KwLoc,
  IdentifierInfo *Ident, SourceLocation IdentLoc,
  std::vector<hlsl::UnusualAnnotation *>& BufferAttributes,
  SourceLocation LBrace)
{
  // For anonymous namespace, take the location of the left brace.
  DeclContext* lexicalParent = getCurLexicalContext();
  clang::HLSLBufferDecl *result = HLSLBufferDecl::Create(
      Context, lexicalParent, cbuffer, /*isConstantBufferView*/ false, KwLoc,
      Ident, IdentLoc, BufferAttributes, LBrace);

  // Keep track of the currently active buffer.
  HLSLBuffers.push_back(result);

  // Validate unusual annotations and emit diagnostics.
  DiagnoseUnusualAnnotationsForHLSL(*this, BufferAttributes);
  auto && unusualIter = BufferAttributes.begin();
  auto && unusualEnd = BufferAttributes.end();
  char expectedRegisterType = cbuffer ? 'b' : 't';
  for (; unusualIter != unusualEnd; ++unusualIter) {
    switch ((*unusualIter)->getKind()) {
    case hlsl::UnusualAnnotation::UA_ConstantPacking: {
      hlsl::ConstantPacking* constantPacking = cast<hlsl::ConstantPacking>(*unusualIter);
      Diag(constantPacking->Loc, diag::err_hlsl_unsupported_buffer_packoffset);
      break;
    }
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment* registerAssignment = cast<hlsl::RegisterAssignment>(*unusualIter);

      if (registerAssignment->isSpaceOnly())
        continue;

      if (registerAssignment->RegisterType != expectedRegisterType && registerAssignment->RegisterType != toupper(expectedRegisterType)) {
        Diag(registerAssignment->Loc, diag::err_hlsl_incorrect_bind_semantic) << (cbuffer ? "'b'" : "'t'");
      } else if (registerAssignment->ShaderProfile.size() > 0) {
        Diag(registerAssignment->Loc, diag::err_hlsl_unsupported_buffer_slot_target_specific);
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl: {
      // Ignore semantic declarations.
      break;
    }
    }
  }

  PushOnScopeChains(result, bufferScope);
  PushDeclContext(bufferScope, result);

  ActOnDocumentableDecl(result);

  return result;
}

void Sema::ActOnFinishHLSLBuffer(Decl *Dcl, SourceLocation RBrace)
{
  DXASSERT_NOMSG(Dcl != nullptr);
  DXASSERT(Dcl == HLSLBuffers.back(), "otherwise push/pop is incorrect");
  dyn_cast<HLSLBufferDecl>(Dcl)->setRBraceLoc(RBrace);
  HLSLBuffers.pop_back();
  PopDeclContext();
}

Decl* Sema::getActiveHLSLBuffer() const
{
  return HLSLBuffers.empty() ? nullptr : HLSLBuffers.back();
}

Decl *Sema::ActOnHLSLBufferView(Scope *bufferScope, SourceLocation KwLoc,
                            DeclGroupPtrTy &dcl, bool iscbuf) {
  DXASSERT(nullptr == HLSLBuffers.back(), "otherwise push/pop is incorrect");
  HLSLBuffers.pop_back();
  DXASSERT(HLSLBuffers.empty(), "otherwise push/pop is incorrect");

  Decl *decl = dcl.get().getSingleDecl();
  NamedDecl *namedDecl = cast<NamedDecl>(decl);
  IdentifierInfo *Ident = namedDecl->getIdentifier();

  // No anonymous namespace for ConstantBuffer, take the location of the decl.
  SourceLocation Loc = decl->getLocation();

  // Prevent array type in template.  The only way to specify an array in the template type
  // is to use a typedef, so we will strip non-typedef arrays off, since these are the legal
  // array dimensions for the CBV/TBV, and if any array type remains, that is illegal.
  QualType declType = cast<VarDecl>(namedDecl)->getType();
  while (declType->isArrayType() && declType->getTypeClass() != Type::TypeClass::Typedef) {
    const ArrayType *arrayType = declType->getAsArrayTypeUnsafe();
    declType = arrayType->getElementType();
  }
  // Check to make that sure only structs are allowed as parameter types for
  // ConstantBuffer and TextureBuffer.
  if (!declType->isStructureType()) {
    Diag(decl->getLocStart(),
         diag::err_hlsl_typeintemplateargument_requires_struct)
        << declType;
    return nullptr;
  }

  std::vector<hlsl::UnusualAnnotation *> hlslAttrs;

  DeclContext *lexicalParent = getCurLexicalContext();
  clang::HLSLBufferDecl *result = HLSLBufferDecl::Create(
      Context, lexicalParent, iscbuf, /*isConstantBufferView*/ true,
      KwLoc, Ident, Loc, hlslAttrs, Loc);

  // set relation
  namedDecl->setDeclContext(result);
  result->addDecl(namedDecl);
  // move attribute from constant to constant buffer
  result->setUnusualAnnotations(namedDecl->getUnusualAnnotations());
  namedDecl->setUnusualAnnotations(hlslAttrs);

  return result;
}

bool Sema::IsOnHLSLBufferView() {
  // nullptr will not pushed for cbuffer.
  return !HLSLBuffers.empty() && getActiveHLSLBuffer() == nullptr;
}
void Sema::ActOnStartHLSLBufferView() {
  // Push nullptr to mark HLSLBufferView.
  DXASSERT(HLSLBuffers.empty(), "otherwise push/pop is incorrect");
  HLSLBuffers.emplace_back(nullptr);
}

HLSLBufferDecl::HLSLBufferDecl(
    DeclContext *DC, bool cbuffer, bool cbufferView, SourceLocation KwLoc,
    IdentifierInfo *Id, SourceLocation IdLoc,
    std::vector<hlsl::UnusualAnnotation *> &BufferAttributes,
    SourceLocation LBrace)
    : NamedDecl(Decl::HLSLBuffer, DC, IdLoc, DeclarationName(Id)),
      DeclContext(Decl::HLSLBuffer), LBraceLoc(LBrace), KwLoc(KwLoc),
      IsCBuffer(cbuffer), IsConstantBufferView(cbufferView) {
  if (!BufferAttributes.empty()) {
    setUnusualAnnotations(UnusualAnnotation::CopyToASTContextArray(
        getASTContext(), BufferAttributes.data(), BufferAttributes.size()));
  }
}

HLSLBufferDecl *
HLSLBufferDecl::Create(ASTContext &C, DeclContext *lexicalParent, bool cbuffer,
                       bool constantbuffer, SourceLocation KwLoc,
                       IdentifierInfo *Id, SourceLocation IdLoc,
                       std::vector<hlsl::UnusualAnnotation *> &BufferAttributes,
                       SourceLocation LBrace) {
  DeclContext *DC = C.getTranslationUnitDecl();
  HLSLBufferDecl *result = ::new (C) HLSLBufferDecl(
      DC, cbuffer, constantbuffer, KwLoc, Id, IdLoc, BufferAttributes, LBrace);
  if (DC != lexicalParent) {
    result->setLexicalDeclContext(lexicalParent);
  }

  return result;
}

const char *HLSLBufferDecl::getDeclKindName() const {
  static const char *HLSLBufferNames[] = {"tbuffer", "cbuffer", "TextureBuffer",
                                          "ConstantBuffer"};
  unsigned index = (unsigned ) isCBuffer() | (isConstantBufferView()) << 1;
  return HLSLBufferNames[index];
}

void Sema::TransferUnusualAttributes(Declarator &D, NamedDecl *NewDecl) {
  assert(NewDecl != nullptr);

  if (!getLangOpts().HLSL) {
    return;
  }

  if (!D.UnusualAnnotations.empty()) {
    NewDecl->setUnusualAnnotations(UnusualAnnotation::CopyToASTContextArray(
        getASTContext(), D.UnusualAnnotations.data(),
        D.UnusualAnnotations.size()));
    D.UnusualAnnotations.clear();
  }
}

/// Checks whether a usage attribute is compatible with those seen so far and
/// maintains history.
static bool IsUsageAttributeCompatible(AttributeList::Kind kind, bool &usageIn,
                                       bool &usageOut) {
  switch (kind) {
  case AttributeList::AT_HLSLIn:
    if (usageIn)
      return false;
    usageIn = true;
    break;
  case AttributeList::AT_HLSLOut:
    if (usageOut)
      return false;
    usageOut = true;
    break;
  default:
    assert(kind == AttributeList::AT_HLSLInOut);
    if (usageOut || usageIn)
      return false;
    usageIn = usageOut = true;
    break;
  }
  return true;
}

// Diagnose valid/invalid modifiers for HLSL.
bool Sema::DiagnoseHLSLDecl(Declarator &D, DeclContext *DC, Expr *BitWidth,
                            TypeSourceInfo *TInfo, bool isParameter) {
  assert(getLangOpts().HLSL &&
         "otherwise this is called without checking language first");

  // NOTE: some tests may declare templates.
  if (DC->isNamespace() || DC->isDependentContext()) return true;

  DeclSpec::SCS storage = D.getDeclSpec().getStorageClassSpec();
  assert(!DC->isClosure() && "otherwise parser accepted closure syntax instead of failing with a syntax error");
  assert(!DC->isDependentContext() && "otherwise parser accepted a template instead of failing with a syntax error");
  assert(!DC->isNamespace() && "otherwise parser accepted a namespace instead of failing a syntax error");

  bool result = true;
  bool isTypedef = storage == DeclSpec::SCS_typedef;
  bool isFunction = D.isFunctionDeclarator() && !DC->isRecord();
  bool isLocalVar = DC->isFunctionOrMethod() && !isFunction && !isTypedef;
  bool isGlobal = !isParameter && !isTypedef && !isFunction && (DC->isTranslationUnit() || DC->getDeclKind() == Decl::HLSLBuffer);
  bool isMethod = DC->isRecord() && D.isFunctionDeclarator() && !isTypedef;
  bool isField = DC->isRecord() && !D.isFunctionDeclarator() && !isTypedef;

  bool isConst = D.getDeclSpec().getTypeQualifiers() & DeclSpec::TQ::TQ_const;
  bool isVolatile = D.getDeclSpec().getTypeQualifiers() & DeclSpec::TQ::TQ_volatile;
  bool isStatic = storage == DeclSpec::SCS::SCS_static;
  bool isExtern = storage == DeclSpec::SCS::SCS_extern;

  bool hasSignSpec = D.getDeclSpec().getTypeSpecSign() != DeclSpec::TSS::TSS_unspecified;

  // Function declarations are not allowed in parameter declaration
  // TODO : Remove this check once we support function declarations/pointers in HLSL
  if (isParameter && isFunction) {
      Diag(D.getLocStart(), diag::err_hlsl_func_in_func_decl);
      D.setInvalidType();
      return false;
  }

  assert(
    (1 == (isLocalVar ? 1 : 0) + (isGlobal ? 1 : 0) + (isField ? 1 : 0) +
    (isTypedef ? 1 : 0) + (isFunction ? 1 : 0) + (isMethod ? 1 : 0) +
    (isParameter ? 1 : 0))
    && "exactly one type of declarator is being processed");

  // qt/pType captures either the type being modified, or the return type in the
  // case of a function (or method).
  QualType qt = TInfo->getType();
  const Type* pType = qt.getTypePtrOrNull();
  HLSLExternalSource *hlslSource = HLSLExternalSource::FromSema(this);

  // Early checks - these are not simple attribution errors, but constructs that
  // are fundamentally unsupported,
  // and so we avoid errors that might indicate they can be repaired.
  if (DC->isRecord()) {
    unsigned int nestedDiagId = 0;
    if (isTypedef) {
      nestedDiagId = diag::err_hlsl_unsupported_nested_typedef;
    }

    if (isField && pType && pType->isIncompleteArrayType()) {
      nestedDiagId = diag::err_hlsl_unsupported_incomplete_array;
    }

    if (nestedDiagId) {
      Diag(D.getLocStart(), nestedDiagId);
      D.setInvalidType();
      return false;
    }
  }

  // String and subobject declarations are supported only as top level global variables.
  // Const and static modifiers are implied - add them if missing.
  if ((hlsl::IsStringType(qt) || hlslSource->IsSubobjectType(qt)) && !D.isInvalidType()) {
    // string are supported only as top level global variables
    if (!DC->isTranslationUnit()) {
      Diag(D.getLocStart(), diag::err_hlsl_object_not_global) << (int)hlsl::IsStringType(qt);
      result = false;
    }
    if (isExtern) {
      Diag(D.getLocStart(), diag::err_hlsl_object_extern_not_supported) << (int)hlsl::IsStringType(qt);
      result = false;
    }
    const char *PrevSpec = nullptr;
    unsigned DiagID = 0;
    if (!isStatic) {
      D.getMutableDeclSpec().SetStorageClassSpec(*this, DeclSpec::SCS_static, D.getLocStart(), PrevSpec, DiagID, Context.getPrintingPolicy());
      isStatic = true;
    }
    if (!isConst) {
      D.getMutableDeclSpec().SetTypeQual(DeclSpec::TQ_const, D.getLocStart(), PrevSpec, DiagID, getLangOpts());
      isConst = true;
    }
  }

  const char* declarationType =
    (isLocalVar) ? "local variable" :
    (isTypedef) ? "typedef" :
    (isFunction) ? "function" :
    (isMethod) ? "method" :
    (isGlobal) ? "global variable" :
    (isParameter) ? "parameter" :
    (isField) ? "field" : "<unknown>";

  if (pType && D.isFunctionDeclarator()) {
    const FunctionProtoType *pFP = pType->getAs<FunctionProtoType>();
    if (pFP) {
      qt = pFP->getReturnType();
      pType = qt.getTypePtrOrNull();

      // prohibit string as a return type
      if (hlsl::IsStringType(qt)) {
        static const unsigned selectReturnValueIdx = 2;
        Diag(D.getLocStart(), diag::err_hlsl_unsupported_string_decl) << selectReturnValueIdx;
        D.setInvalidType();
      }
    }
  }

  // Check for deprecated effect object type here, warn, and invalidate decl
  bool bDeprecatedEffectObject = false;
  bool bIsObject = false;
  if (hlsl::IsObjectType(this, qt, &bDeprecatedEffectObject)) {
    bIsObject = true;
    if (bDeprecatedEffectObject) {
      Diag(D.getLocStart(), diag::warn_hlsl_effect_object);
      D.setInvalidType();
      return false;
    }
    // Add methods if not ready.
    hlslSource->AddHLSLObjectMethodsIfNotReady(qt);
  } else if (qt->isArrayType()) {
    QualType eltQt(qt->getArrayElementTypeNoTypeQual(), 0);
    while (eltQt->isArrayType())
      eltQt = QualType(eltQt->getArrayElementTypeNoTypeQual(), 0);

    if (hlsl::IsObjectType(this, eltQt, &bDeprecatedEffectObject)) {
      // Add methods if not ready.
      hlslSource->AddHLSLObjectMethodsIfNotReady(eltQt);
    }
  }

  if (isExtern) {
    if (!(isFunction || isGlobal)) {
      Diag(D.getLocStart(), diag::err_hlsl_varmodifierna) << "'extern'"
                                                          << declarationType;
      result = false;
    }
  }

  if (isStatic) {
    if (!(isLocalVar || isGlobal || isFunction || isMethod || isField)) {
      Diag(D.getLocStart(), diag::err_hlsl_varmodifierna) << "'static'"
                                                          << declarationType;
      result = false;
    }
  }

  if (isVolatile) {
    if (!(isLocalVar || isTypedef)) {
      Diag(D.getLocStart(), diag::err_hlsl_varmodifierna) << "'volatile'"
                                                          << declarationType;
      result = false;
    }
  }

  if (isConst) {
    if (isField && !isStatic) {
      Diag(D.getLocStart(), diag::err_hlsl_varmodifierna) << "'const'"
                                                          << declarationType;
      result = false;
    }
  }

  ArBasicKind basicKind = hlslSource->GetTypeElementKind(qt);

  if (hasSignSpec) {
     ArTypeObjectKind objKind = hlslSource->GetTypeObjectKind(qt);
     // vectors or matrices can only have unsigned integer types.
     if (objKind == AR_TOBJ_MATRIX || objKind == AR_TOBJ_VECTOR || objKind == AR_TOBJ_BASIC || objKind == AR_TOBJ_ARRAY) {
         if (!IS_BASIC_UNSIGNABLE(basicKind)) {
             Diag(D.getLocStart(), diag::err_sema_invalid_sign_spec)
                 << g_ArBasicTypeNames[basicKind];
             result = false;
         }
     }
     else {
         Diag(D.getLocStart(), diag::err_sema_invalid_sign_spec) << g_ArBasicTypeNames[basicKind];
         result = false;
     }
  }

  // Validate attributes
  clang::AttributeList
    *pUniform = nullptr,
    *pUsage = nullptr,
    *pNoInterpolation = nullptr,
    *pLinear = nullptr,
    *pNoPerspective = nullptr,
    *pSample = nullptr,
    *pCentroid = nullptr,
    *pCenter = nullptr,
    *pAnyLinear = nullptr,                   // first linear attribute found
    *pTopology = nullptr;
  bool usageIn = false;
  bool usageOut = false;

  for (clang::AttributeList *pAttr = D.getDeclSpec().getAttributes().getList();
       pAttr != NULL; pAttr = pAttr->getNext()) {
    if (pAttr->isInvalid() || pAttr->isUsedAsTypeAttr())
      continue;

    switch (pAttr->getKind()) {
    case AttributeList::AT_HLSLPrecise: // precise is applicable everywhere.
      break;
    case AttributeList::AT_HLSLShared:
      if (!isGlobal) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }
      if (isStatic) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifiersna)
            << "'static'" << pAttr->getName() << declarationType
            << pAttr->getRange();
        result = false;
      }
      break;
    case AttributeList::AT_HLSLGroupShared:
      if (!isGlobal) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }
      if (isExtern) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifiersna)
            << "'extern'" << pAttr->getName() << declarationType
            << pAttr->getRange();
        result = false;
      }
      break;
    case AttributeList::AT_HLSLGloballyCoherent:
      if (!bIsObject) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << "non-UAV type";
        result = false;
      }
      break;
    case AttributeList::AT_HLSLUniform:
      if (!(isGlobal || isParameter)) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }
      if (isStatic) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifiersna)
            << "'static'" << pAttr->getName() << declarationType
            << pAttr->getRange();
        result = false;
      }
      pUniform = pAttr;
      break;

    case AttributeList::AT_HLSLIn:
    case AttributeList::AT_HLSLOut:
    case AttributeList::AT_HLSLInOut:
      if (!isParameter) {
        Diag(pAttr->getLoc(), diag::err_hlsl_usage_not_on_parameter)
            << pAttr->getName() << pAttr->getRange();
        result = false;
      }
      if (!IsUsageAttributeCompatible(pAttr->getKind(), usageIn, usageOut)) {
        Diag(pAttr->getLoc(), diag::err_hlsl_duplicate_parameter_usages)
            << pAttr->getName() << pAttr->getRange();
        result = false;
      }
      pUsage = pAttr;
      break;

    case AttributeList::AT_HLSLNoInterpolation:
      if (!(isParameter || isField || isFunction)) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }
      if (pNoInterpolation) {
        Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
            << pAttr->getName() << pAttr->getRange();
      }
      pNoInterpolation = pAttr;
      break;

    case AttributeList::AT_HLSLLinear:
    case AttributeList::AT_HLSLCenter:
    case AttributeList::AT_HLSLNoPerspective:
    case AttributeList::AT_HLSLSample:
    case AttributeList::AT_HLSLCentroid:
      if (!(isParameter || isField || isFunction)) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
            << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }

      if (nullptr == pAnyLinear)
        pAnyLinear = pAttr;

      switch (pAttr->getKind()) {
      case AttributeList::AT_HLSLLinear:
        if (pLinear) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
              << pAttr->getName() << pAttr->getRange();
        }
        pLinear = pAttr;
        break;
      case AttributeList::AT_HLSLCenter:
        if (pCenter) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
            << pAttr->getName() << pAttr->getRange();
        }
        pCenter = pAttr;
        break;
      case AttributeList::AT_HLSLNoPerspective:
        if (pNoPerspective) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
              << pAttr->getName() << pAttr->getRange();
        }
        pNoPerspective = pAttr;
        break;
      case AttributeList::AT_HLSLSample:
        if (pSample) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
              << pAttr->getName() << pAttr->getRange();
        }
        pSample = pAttr;
        break;
      case AttributeList::AT_HLSLCentroid:
        if (pCentroid) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
              << pAttr->getName() << pAttr->getRange();
        }
        pCentroid = pAttr;
        break;
      default:
        // Only relevant to the four attribs included in this block.
        break;
      }
      break;

    case AttributeList::AT_HLSLPoint:
    case AttributeList::AT_HLSLLine:
    case AttributeList::AT_HLSLLineAdj:
    case AttributeList::AT_HLSLTriangle:
    case AttributeList::AT_HLSLTriangleAdj:
      if (!(isParameter)) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
          << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }

      if (pTopology) {
        if (pTopology->getKind() == pAttr->getKind()) {
          Diag(pAttr->getLoc(), diag::warn_hlsl_duplicate_specifier)
            << pAttr->getName() << pAttr->getRange();
        } else {
          Diag(pAttr->getLoc(), diag::err_hlsl_varmodifiersna)
            << pAttr->getName() << pTopology->getName()
            << declarationType << pAttr->getRange();
          result = false;
        }
      }
      pTopology = pAttr;
      break;

    case AttributeList::AT_HLSLExport:
      if (!isFunction) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifierna)
          << pAttr->getName() << declarationType << pAttr->getRange();
        result = false;
      }
      if (isStatic) {
        Diag(pAttr->getLoc(), diag::err_hlsl_varmodifiersna)
          << "'static'" << pAttr->getName() << declarationType
          << pAttr->getRange();
        result = false;
      }
      break;

    default:
      break;
    }
  }

  if (pNoInterpolation && pAnyLinear) {
    Diag(pNoInterpolation->getLoc(), diag::err_hlsl_varmodifiersna)
        << pNoInterpolation->getName() << pAnyLinear->getName()
        << declarationType << pNoInterpolation->getRange();
    result = false;
  }
  if (pSample && pCentroid) {
    Diag(pCentroid->getLoc(), diag::warn_hlsl_specifier_overridden)
        << pCentroid->getName() << pSample->getName() << pCentroid->getRange();
  }
  if (pCenter && pCentroid) {
    Diag(pCenter->getLoc(), diag::warn_hlsl_specifier_overridden)
      << pCenter->getName() << pCentroid->getName() << pCenter->getRange();
  }
  if (pSample && pCenter) {
    Diag(pCenter->getLoc(), diag::warn_hlsl_specifier_overridden)
      << pCenter->getName() << pSample->getName() << pCenter->getRange();
  }
  clang::AttributeList *pNonUniformAttr = pAnyLinear ? pAnyLinear : (
    pNoInterpolation ? pNoInterpolation : pTopology);
  if (pUniform && pNonUniformAttr) {
    Diag(pUniform->getLoc(), diag::err_hlsl_varmodifiersna)
        << pNonUniformAttr->getName()
        << pUniform->getName() << declarationType << pUniform->getRange();
    result = false;
  }
  if (pAnyLinear && pTopology) {
    Diag(pAnyLinear->getLoc(), diag::err_hlsl_varmodifiersna)
      << pTopology->getName()
      << pAnyLinear->getName() << declarationType << pAnyLinear->getRange();
    result = false;
  }
  if (pNoInterpolation && pTopology) {
    Diag(pNoInterpolation->getLoc(), diag::err_hlsl_varmodifiersna)
      << pTopology->getName()
      << pNoInterpolation->getName() << declarationType << pNoInterpolation->getRange();
    result = false;
  }
  if (pUniform && pUsage) {
    if (pUsage->getKind() != AttributeList::Kind::AT_HLSLIn) {
      Diag(pUniform->getLoc(), diag::err_hlsl_varmodifiersna)
          << pUsage->getName() << pUniform->getName() << declarationType
          << pUniform->getRange();
      result = false;
    }
  }

  // Validate that stream-ouput objects are marked as inout
  if (isParameter && !(usageIn && usageOut) &&
      (basicKind == ArBasicKind::AR_OBJECT_LINESTREAM ||
       basicKind == ArBasicKind::AR_OBJECT_POINTSTREAM ||
       basicKind == ArBasicKind::AR_OBJECT_TRIANGLESTREAM)) {
    Diag(D.getLocStart(), diag::err_hlsl_missing_inout_attr);
    result = false;
  }

  // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
  // Validate that Vulkan specific feature is only used when targeting SPIR-V
  if (!getLangOpts().SPIRV) {
    if (basicKind == ArBasicKind::AR_OBJECT_VK_SUBPASS_INPUT ||
        basicKind == ArBasicKind::AR_OBJECT_VK_SUBPASS_INPUT_MS) {
      Diag(D.getLocStart(), diag::err_hlsl_vulkan_specific_feature)
          << g_ArBasicTypeNames[basicKind];
      result = false;
    }
  }
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV change ends

  // Disallow bitfields
  if (BitWidth) {
    Diag(BitWidth->getExprLoc(), diag::err_hlsl_bitfields);
    result = false;
  }

  // Validate unusual annotations.
  hlsl::DiagnoseUnusualAnnotationsForHLSL(*this, D.UnusualAnnotations);
  auto && unusualIter = D.UnusualAnnotations.begin();
  auto && unusualEnd = D.UnusualAnnotations.end();
  for (; unusualIter != unusualEnd; ++unusualIter) {
    switch ((*unusualIter)->getKind()) {
    case hlsl::UnusualAnnotation::UA_ConstantPacking: {
      hlsl::ConstantPacking *constantPacking =
          cast<hlsl::ConstantPacking>(*unusualIter);
      if (!isGlobal || HLSLBuffers.size() == 0) {
        Diag(constantPacking->Loc, diag::err_hlsl_packoffset_requires_cbuffer);
        continue;
      }
      if (constantPacking->ComponentOffset > 0) {
        // Validate that this will fit.
        if (!qt.isNull()) {
          hlsl::DiagnosePackingOffset(this, constantPacking->Loc, qt,
                                      constantPacking->ComponentOffset);
        }
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment *registerAssignment =
          cast<hlsl::RegisterAssignment>(*unusualIter);
      if (registerAssignment->IsValid) {
        if (!qt.isNull()) {
          hlsl::DiagnoseRegisterType(this, registerAssignment->Loc, qt,
                                     registerAssignment->RegisterType);
        }
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl: {
      hlsl::SemanticDecl *semanticDecl = cast<hlsl::SemanticDecl>(*unusualIter);
      if (isTypedef || isLocalVar) {
        Diag(semanticDecl->Loc, diag::err_hlsl_varmodifierna)
            << "semantic" << declarationType;
      }
      break;
    }
    }
  }

  if (!result) {
    D.setInvalidType();
  }

  return result;
}

// Diagnose HLSL types on lookup
bool Sema::DiagnoseHLSLLookup(const LookupResult &R) {
  const DeclarationNameInfo declName = R.getLookupNameInfo();
  IdentifierInfo* idInfo = declName.getName().getAsIdentifierInfo();
  if (idInfo) {
    StringRef nameIdentifier = idInfo->getName();
    HLSLScalarType parsedType;
    int rowCount, colCount;
    if (TryParseAny(nameIdentifier.data(), nameIdentifier.size(), &parsedType, &rowCount, &colCount, getLangOpts())) {
      HLSLExternalSource *hlslExternalSource = HLSLExternalSource::FromSema(this);
      hlslExternalSource->WarnMinPrecision(parsedType, R.getNameLoc());
      return hlslExternalSource->DiagnoseHLSLScalarType(parsedType, R.getNameLoc());
    }
  }
  return true;
}

static QualType getUnderlyingType(QualType Type)
{
  while (const TypedefType *TD = dyn_cast<TypedefType>(Type))
  {
    if (const TypedefNameDecl* pDecl = TD->getDecl())
      Type = pDecl->getUnderlyingType();
    else
      break;
  }
  return Type;
}

/// <summary>Return HLSL AttributedType objects if they exist on type.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="type">QualType to inspect.</param>
/// <param name="ppMatrixOrientation">Set pointer to column_major/row_major AttributedType if supplied.</param>
/// <param name="ppNorm">Set pointer to snorm/unorm AttributedType if supplied.</param>
void hlsl::GetHLSLAttributedTypes(
  _In_ clang::Sema* self,
  clang::QualType type, 
  _Inout_opt_ const clang::AttributedType** ppMatrixOrientation, 
  _Inout_opt_ const clang::AttributedType** ppNorm)
{
  if (ppMatrixOrientation)
    *ppMatrixOrientation = nullptr;
  if (ppNorm)
    *ppNorm = nullptr;

  // Note: we clear output pointers once set so we can stop searching
  QualType Desugared = getUnderlyingType(type);
  const AttributedType *AT = dyn_cast<AttributedType>(Desugared);
  while (AT && (ppMatrixOrientation || ppNorm)) {
    AttributedType::Kind Kind = AT->getAttrKind();

    if (Kind == AttributedType::attr_hlsl_row_major ||
        Kind == AttributedType::attr_hlsl_column_major)
    {
      if (ppMatrixOrientation)
      {
        *ppMatrixOrientation = AT;
        ppMatrixOrientation = nullptr;
      }
    }
    else if (Kind == AttributedType::attr_hlsl_unorm ||
             Kind == AttributedType::attr_hlsl_snorm)
    {
      if (ppNorm)
      {
        *ppNorm = AT;
        ppNorm = nullptr;
      }
    }

    Desugared = getUnderlyingType(AT->getEquivalentType());
    AT = dyn_cast<AttributedType>(Desugared);
  }

  // Unwrap component type on vector or matrix and check snorm/unorm
  Desugared = getUnderlyingType(hlsl::GetOriginalElementType(self, Desugared));
  AT = dyn_cast<AttributedType>(Desugared);
  while (AT && ppNorm) {
    AttributedType::Kind Kind = AT->getAttrKind();

    if (Kind == AttributedType::attr_hlsl_unorm ||
      Kind == AttributedType::attr_hlsl_snorm)
    {
      *ppNorm = AT;
      ppNorm = nullptr;
    }

    Desugared = getUnderlyingType(AT->getEquivalentType());
    AT = dyn_cast<AttributedType>(Desugared);
  }
}

/// <summary>Returns true if QualType is an HLSL Matrix type.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="type">QualType to check.</param>
bool hlsl::IsMatrixType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type)
{
  return HLSLExternalSource::FromSema(self)->GetTypeObjectKind(type) == AR_TOBJ_MATRIX;
}

/// <summary>Returns true if QualType is an HLSL Vector type.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="type">QualType to check.</param>
bool hlsl::IsVectorType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type)
{
  return HLSLExternalSource::FromSema(self)->GetTypeObjectKind(type) == AR_TOBJ_VECTOR;
}

/// <summary>Get element type for an HLSL Matrix or Vector, preserving AttributedType.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="type">Matrix or Vector type.</param>
clang::QualType hlsl::GetOriginalMatrixOrVectorElementType(
  _In_ clang::QualType type)
{
  // TODO: Determine if this is really the best way to get the matrix/vector specialization
  // without losing the AttributedType on the template parameter
  if (const Type* pType = type.getTypePtrOrNull()) {
    // A non-dependent template specialization type is always "sugar",
    // typically for a RecordType.  For example, a class template
    // specialization type of @c vector<int> will refer to a tag type for
    // the instantiation @c std::vector<int, std::allocator<int>>.
    if (const TemplateSpecializationType* pTemplate = pType->getAs<TemplateSpecializationType>()) {
      // If we have enough arguments, pull them from the template directly, rather than doing
      // the extra lookups.
      if (pTemplate->getNumArgs() > 0)
        return pTemplate->getArg(0).getAsType();

      QualType templateRecord = pTemplate->desugar();
      const Type *pTemplateRecordType = templateRecord.getTypePtr();
      if (pTemplateRecordType) {
        const TagType *pTemplateTagType = pTemplateRecordType->getAs<TagType>();
        if (pTemplateTagType) {
          const ClassTemplateSpecializationDecl *specializationDecl =
              dyn_cast_or_null<ClassTemplateSpecializationDecl>(
                  pTemplateTagType->getDecl());
          if (specializationDecl) {
            return specializationDecl->getTemplateArgs()[0].getAsType();
          }
        }
      }
    }
  }
  return QualType();
}

/// <summary>Get element type, preserving AttributedType, if vector or matrix, otherwise return the type unmodified.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="type">Input type.</param>
clang::QualType hlsl::GetOriginalElementType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type)
{
  ArTypeObjectKind Kind = HLSLExternalSource::FromSema(self)->GetTypeObjectKind(type);
  if (Kind == AR_TOBJ_MATRIX || Kind == AR_TOBJ_VECTOR) {
    return GetOriginalMatrixOrVectorElementType(type);
  }
  return type;
}

void hlsl::CustomPrintHLSLAttr(const clang::Attr *A, llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy, unsigned int Indentation) {
  switch (A->getKind()) {

  // Parameter modifiers
  case clang::attr::HLSLIn:
    Out << "in ";
    break;

  case clang::attr::HLSLInOut:
    Out << "inout ";
    break;

  case clang::attr::HLSLOut:
    Out << "out ";
    break;
  
  // Interpolation modifiers
  case clang::attr::HLSLLinear:
    Out << "linear ";
    break;

  case clang::attr::HLSLCenter:
    Out << "center ";
    break;

  case clang::attr::HLSLCentroid:
    Out << "centroid ";
    break;

  case clang::attr::HLSLNoInterpolation:
    Out << "nointerpolation ";
    break;

  case clang::attr::HLSLNoPerspective:
    Out << "noperspective ";
    break;

  case clang::attr::HLSLSample:
    Out << "sample ";
    break;
  
  // Function attributes
  case clang::attr::HLSLClipPlanes:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLClipPlanesAttr *ACast = static_cast<HLSLClipPlanesAttr*>(noconst);
    
    if (!ACast->getClipPlane1()) 
      break;
    
    Indent(Indentation, Out);
    Out << "[clipplanes(";
    ACast->getClipPlane1()->printPretty(Out, 0, Policy);
    PrintClipPlaneIfPresent(ACast->getClipPlane2(), Out, Policy);
    PrintClipPlaneIfPresent(ACast->getClipPlane3(), Out, Policy);
    PrintClipPlaneIfPresent(ACast->getClipPlane4(), Out, Policy);
    PrintClipPlaneIfPresent(ACast->getClipPlane5(), Out, Policy);
    PrintClipPlaneIfPresent(ACast->getClipPlane6(), Out, Policy);
    Out << ")]\n";
    
    break;
  }
  
  case clang::attr::HLSLDomain:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLDomainAttr *ACast = static_cast<HLSLDomainAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[domain(\"" << ACast->getDomainType() << "\")]\n";
    break;
  }
  
  case clang::attr::HLSLEarlyDepthStencil:
    Indent(Indentation, Out);
    Out << "[earlydepthstencil]\n";
    break;
  
  case clang::attr::HLSLInstance: //TODO - test 
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLInstanceAttr *ACast = static_cast<HLSLInstanceAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[instance(" << ACast->getCount() << ")]\n";
    break;
  }
  
  case clang::attr::HLSLMaxTessFactor: //TODO - test
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLMaxTessFactorAttr *ACast = static_cast<HLSLMaxTessFactorAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[maxtessfactor(" << ACast->getFactor() << ")]\n";
    break;
  }
  
  case clang::attr::HLSLNumThreads: //TODO - test
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLNumThreadsAttr *ACast = static_cast<HLSLNumThreadsAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[numthreads(" << ACast->getX() << ", " << ACast->getY() << ", " << ACast->getZ() << ")]\n";
    break;
  }
  
  case clang::attr::HLSLRootSignature:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLRootSignatureAttr *ACast = static_cast<HLSLRootSignatureAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[RootSignature(" << ACast->getSignatureName() << ")]\n";
    break;
  }

  case clang::attr::HLSLOutputControlPoints:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLOutputControlPointsAttr *ACast = static_cast<HLSLOutputControlPointsAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[outputcontrolpoints(" << ACast->getCount() << ")]\n";
    break;
  }
  
  case clang::attr::HLSLOutputTopology:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLOutputTopologyAttr *ACast = static_cast<HLSLOutputTopologyAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[outputtopology(\"" << ACast->getTopology() << "\")]\n";
    break;
  }
  
  case clang::attr::HLSLPartitioning:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLPartitioningAttr *ACast = static_cast<HLSLPartitioningAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[partitioning(\"" << ACast->getScheme() << "\")]\n";
    break;
  }
  
  case clang::attr::HLSLPatchConstantFunc:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLPatchConstantFuncAttr *ACast = static_cast<HLSLPatchConstantFuncAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[patchconstantfunc(\"" << ACast->getFunctionName() << "\")]\n";
    break;
  }

  case clang::attr::HLSLShader:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLShaderAttr *ACast = static_cast<HLSLShaderAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[shader(\"" << ACast->getStage() << "\")]\n";
    break;
  }

  case clang::attr::HLSLExperimental:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLExperimentalAttr *ACast = static_cast<HLSLExperimentalAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[experimental(\"" << ACast->getName() << "\", \"" << ACast->getValue() << "\")]\n";
    break;
  }

  case clang::attr::HLSLMaxVertexCount:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLMaxVertexCountAttr *ACast = static_cast<HLSLMaxVertexCountAttr*>(noconst);
    Indent(Indentation, Out);
    Out << "[maxvertexcount(" << ACast->getCount() << ")]\n";
    break;
  }

  case clang::attr::NoInline:
    Indent(Indentation, Out);
    Out << "[noinline]\n";
    break;

  case clang::attr::HLSLExport:
    Indent(Indentation, Out);
    Out << "export\n";
    break;

    // Statement attributes
  case clang::attr::HLSLAllowUAVCondition:
    Indent(Indentation, Out);
    Out << "[allow_uav_condition]\n";
    break;
  
  case clang::attr::HLSLBranch:
    Indent(Indentation, Out);
    Out << "[branch]\n";
    break;
  
  case clang::attr::HLSLCall:
    Indent(Indentation, Out);
    Out << "[call]\n";
    break;
  
  case clang::attr::HLSLFastOpt:
    Indent(Indentation, Out);
    Out << "[fastopt]\n";
    break;
  
  case clang::attr::HLSLFlatten:
    Indent(Indentation, Out);
    Out << "[flatten]\n";
    break;
  
  case clang::attr::HLSLForceCase:
    Indent(Indentation, Out);
    Out << "[forcecase]\n";
    break;
  
  case clang::attr::HLSLLoop:
    Indent(Indentation, Out);
    Out << "[loop]\n";
    break;

  case clang::attr::HLSLUnroll:
  {
    Attr * noconst = const_cast<Attr*>(A);
    HLSLUnrollAttr *ACast = static_cast<HLSLUnrollAttr*>(noconst);
    Indent(Indentation, Out);
    if (ACast->getCount() == 0)
      Out << "[unroll]\n";
    else
      Out << "[unroll(" << ACast->getCount() << ")]\n";
    break;
  }
  
  // Variable modifiers
  case clang::attr::HLSLGroupShared:
    Out << "groupshared ";
    break;
  
  case clang::attr::HLSLPrecise:
    Out << "precise ";
    break;
  
  case clang::attr::HLSLSemantic: // TODO: Consider removing HLSLSemantic attribute
    break;
  
  case clang::attr::HLSLShared:
    Out << "shared ";
    break;
  
  case clang::attr::HLSLUniform:
    Out << "uniform ";
    break;
  
  // These four cases are printed in TypePrinter::printAttributedBefore
  case clang::attr::HLSLColumnMajor:  
  case clang::attr::HLSLRowMajor:
  case clang::attr::HLSLSnorm:
  case clang::attr::HLSLUnorm:
    break;

  case clang::attr::HLSLPoint:
    Out << "point ";
    break;

  case clang::attr::HLSLLine:
    Out << "line ";
    break;

  case clang::attr::HLSLLineAdj:
    Out << "lineadj ";
    break;

  case clang::attr::HLSLTriangle:
    Out << "triangle ";
    break;

  case clang::attr::HLSLTriangleAdj:
    Out << "triangleadj ";
    break;

  case clang::attr::HLSLGloballyCoherent:
    Out << "globallycoherent ";
    break;

  default:
    A->printPretty(Out, Policy);
    break;
  }
}

bool hlsl::IsHLSLAttr(clang::attr::Kind AttrKind) {
  switch (AttrKind){
  case clang::attr::HLSLAllowUAVCondition:
  case clang::attr::HLSLBranch:
  case clang::attr::HLSLCall:
  case clang::attr::HLSLCentroid:
  case clang::attr::HLSLClipPlanes:
  case clang::attr::HLSLColumnMajor:
  case clang::attr::HLSLDomain:
  case clang::attr::HLSLEarlyDepthStencil:
  case clang::attr::HLSLFastOpt:
  case clang::attr::HLSLFlatten:
  case clang::attr::HLSLForceCase:
  case clang::attr::HLSLGroupShared:
  case clang::attr::HLSLIn:
  case clang::attr::HLSLInOut:
  case clang::attr::HLSLInstance:
  case clang::attr::HLSLLinear:
  case clang::attr::HLSLCenter:
  case clang::attr::HLSLLoop:
  case clang::attr::HLSLMaxTessFactor:
  case clang::attr::HLSLNoInterpolation:
  case clang::attr::HLSLNoPerspective:
  case clang::attr::HLSLNumThreads:
  case clang::attr::HLSLRootSignature:
  case clang::attr::HLSLOut:
  case clang::attr::HLSLOutputControlPoints:
  case clang::attr::HLSLOutputTopology:
  case clang::attr::HLSLPartitioning:
  case clang::attr::HLSLPatchConstantFunc:
  case clang::attr::HLSLMaxVertexCount:
  case clang::attr::HLSLPrecise:
  case clang::attr::HLSLRowMajor:
  case clang::attr::HLSLSample:
  case clang::attr::HLSLSemantic:
  case clang::attr::HLSLShared:
  case clang::attr::HLSLSnorm:
  case clang::attr::HLSLUniform:
  case clang::attr::HLSLUnorm:
  case clang::attr::HLSLUnroll:
  case clang::attr::HLSLPoint:
  case clang::attr::HLSLLine:
  case clang::attr::HLSLLineAdj:
  case clang::attr::HLSLTriangle:
  case clang::attr::HLSLTriangleAdj:
  case clang::attr::HLSLGloballyCoherent:
  case clang::attr::NoInline:
  case clang::attr::HLSLExport:
  case clang::attr::VKBinding:
  case clang::attr::VKBuiltIn:
  case clang::attr::VKConstantId:
  case clang::attr::VKCounterBinding:
  case clang::attr::VKIndex:
  case clang::attr::VKInputAttachmentIndex:
  case clang::attr::VKLocation:
  case clang::attr::VKOffset:
  case clang::attr::VKPushConstant:
  case clang::attr::VKShaderRecordNV:
    return true;
  default:
    // Only HLSL/VK Attributes return true. Only used for printPretty(), which doesn't support them.
    break;
  }
  
  return false;
}

void hlsl::PrintClipPlaneIfPresent(clang::Expr *ClipPlane, llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy) {
  if (ClipPlane) {
    Out << ", ";
    ClipPlane->printPretty(Out, 0, Policy);
  }
}

bool hlsl::IsObjectType(
  _In_ clang::Sema* self,
  _In_ clang::QualType type,
  _Inout_opt_ bool *isDeprecatedEffectObject)
{
  HLSLExternalSource *pExternalSource = HLSLExternalSource::FromSema(self);
  if (pExternalSource && pExternalSource->GetTypeObjectKind(type) == AR_TOBJ_OBJECT) {
    if (isDeprecatedEffectObject)
      *isDeprecatedEffectObject = pExternalSource->GetTypeElementKind(type) == AR_OBJECT_LEGACY_EFFECT;
    return true;
  }
  if (isDeprecatedEffectObject)
    *isDeprecatedEffectObject = false;
  return false;
}

bool hlsl::CanConvert(
  _In_ clang::Sema* self, 
  clang::SourceLocation loc,
  _In_ clang::Expr* sourceExpr, 
  clang::QualType target,
  bool explicitConversion,
  _Inout_opt_ clang::StandardConversionSequence* standard)
{
  return HLSLExternalSource::FromSema(self)->CanConvert(loc, sourceExpr, target, explicitConversion, nullptr, standard);
}

void hlsl::Indent(unsigned int Indentation, llvm::raw_ostream &Out)
{
  for (unsigned i = 0; i != Indentation; ++i)
    Out << "  ";
}

void hlsl::RegisterIntrinsicTable(_In_ clang::ExternalSemaSource* self, _In_ IDxcIntrinsicTable* table)
{
  DXASSERT_NOMSG(self != nullptr);
  DXASSERT_NOMSG(table != nullptr);

  HLSLExternalSource* source = (HLSLExternalSource*)self;
  source->RegisterIntrinsicTable(table);
}

clang::QualType hlsl::CheckVectorConditional(
  _In_ clang::Sema* self,
  _In_ clang::ExprResult &Cond,
  _In_ clang::ExprResult &LHS,
  _In_ clang::ExprResult &RHS,
  _In_ clang::SourceLocation QuestionLoc)
{
  return HLSLExternalSource::FromSema(self)->CheckVectorConditional(Cond, LHS, RHS, QuestionLoc);
}

bool IsTypeNumeric(_In_ clang::Sema* self, _In_ clang::QualType &type) {
  UINT count;
  return HLSLExternalSource::FromSema(self)->IsTypeNumeric(type, &count);
}

void Sema::CheckHLSLArrayAccess(const Expr *expr) {
  DXASSERT_NOMSG(isa<CXXOperatorCallExpr>(expr));
  const CXXOperatorCallExpr *OperatorCallExpr = cast<CXXOperatorCallExpr>(expr);
  DXASSERT_NOMSG(OperatorCallExpr->getOperator() == OverloadedOperatorKind::OO_Subscript);

  const Expr *RHS = OperatorCallExpr->getArg(1); // first subscript expression
  llvm::APSInt index;
  if (RHS->EvaluateAsInt(index, Context)) {
      int64_t intIndex = index.getLimitedValue();
      const QualType LHSQualType = OperatorCallExpr->getArg(0)->getType();
      if (IsVectorType(this, LHSQualType)) {
          uint32_t vectorSize = GetHLSLVecSize(LHSQualType);
          // If expression is a double two subscript operator for matrix (e.g x[0][1])
          // we also have to check the first subscript oprator by recursively calling
          // this funciton for the first CXXOperatorCallExpr
          if (isa<CXXOperatorCallExpr>(OperatorCallExpr->getArg(0))) {
              CheckHLSLArrayAccess(cast<CXXOperatorCallExpr>(OperatorCallExpr->getArg(0)));
          }
          if (intIndex < 0 || (uint32_t)intIndex >= vectorSize) {
              Diag(RHS->getExprLoc(),
                  diag::err_hlsl_vector_element_index_out_of_bounds)
                  << (int)intIndex;
          }
      }
      else if (IsMatrixType(this, LHSQualType)) {
          uint32_t rowCount, colCount;
          GetHLSLMatRowColCount(LHSQualType, rowCount, colCount);
          if (intIndex < 0 || (uint32_t)intIndex >= rowCount) {
              Diag(RHS->getExprLoc(), diag::err_hlsl_matrix_row_index_out_of_bounds)
                  << (int)intIndex;
          }
      }
  }
}

clang::QualType ApplyTypeSpecSignToParsedType(
    _In_ clang::Sema* self,
    _In_ clang::QualType &type,
    _In_ clang::TypeSpecifierSign TSS, 
    _In_ clang::SourceLocation Loc
)
{
    return HLSLExternalSource::FromSema(self)->ApplyTypeSpecSignToParsedType(type, TSS, Loc);
}
