///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilMetadataHelper.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helper to serialize/desialize metadata for DxilModule.                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/DXIL/DxilConstants.h"
#include <memory>
#include <string>
#include <vector>

namespace llvm {
class LLVMContext;
class Module;
class Function;
class Instruction;
class Value;
class MDOperand;
class Metadata;
class ConstantAsMetadata;
class MDTuple;
class MDNode;
class NamedMDNode;
class GlobalVariable;
class StringRef;
}

namespace hlsl {

class ShaderModel;
class DxilSignature;
struct DxilEntrySignature;
class DxilSignatureElement;
class DxilModule;
class DxilResourceBase;
class DxilCBuffer;
class DxilResource;
class DxilSampler;
class DxilTypeSystem;
class DxilStructAnnotation;
class DxilFieldAnnotation;
class DxilFunctionAnnotation;
class DxilParameterAnnotation;
class RootSignatureHandle;
struct DxilFunctionProps;
class DxilSubobjects;
class DxilSubobject;

/// Use this class to manipulate DXIL-spcific metadata.
// In our code, only DxilModule and HLModule should use this class.
class DxilMDHelper {
public:
  //
  // Constants for metadata names and field positions.
  //

  // Dxil version.
  static const char kDxilVersionMDName[];
  static const unsigned kDxilVersionNumFields = 2;
  static const unsigned kDxilVersionMajorIdx  = 0;  // DXIL version major.
  static const unsigned kDxilVersionMinorIdx  = 1;  // DXIL version minor.

  // Shader model.
  static const char kDxilShaderModelMDName[];
  static const unsigned kDxilShaderModelNumFields = 3;
  static const unsigned kDxilShaderModelTypeIdx   = 0;  // Shader type (vs,ps,cs,gs,ds,hs).
  static const unsigned kDxilShaderModelMajorIdx  = 1;  // Shader model major.
  static const unsigned kDxilShaderModelMinorIdx  = 2;  // Shader model minor.

  // Intermediate codegen/optimizer options, not valid in final DXIL module.
  static const char kDxilIntermediateOptionsMDName[];
  static const unsigned kDxilIntermediateOptionsFlags = 0;  // Unique element ID.

  // Entry points.
  static const char kDxilEntryPointsMDName[];

  // Root Signature, for intermediate use, not valid in final DXIL module.
  static const char kDxilRootSignatureMDName[];

  // ViewId state.
  static const char kDxilViewIdStateMDName[];

  // Subobjects
  static const char kDxilSubobjectsMDName[];

  // Source info.
  static const char kDxilSourceContentsMDName[];
  static const char kDxilSourceDefinesMDName[];
  static const char kDxilSourceMainFileNameMDName[];
  static const char kDxilSourceArgsMDName[];

  static const unsigned kDxilEntryPointNumFields  = 5;
  static const unsigned kDxilEntryPointFunction   = 0;  // Entry point function symbol.
  static const unsigned kDxilEntryPointName       = 1;  // Entry point unmangled name.
  static const unsigned kDxilEntryPointSignatures = 2;  // Entry point signature tuple.
  static const unsigned kDxilEntryPointResources  = 3;  // Entry point resource tuple.
  static const unsigned kDxilEntryPointProperties = 4;  // Entry point properties tuple.

  // Signatures.
  static const unsigned kDxilNumSignatureFields     = 3;
  static const unsigned kDxilInputSignature         = 0;  // Shader input signature.
  static const unsigned kDxilOutputSignature        = 1;  // Shader output signature.
  static const unsigned kDxilPatchConstantSignature = 2;  // Shader patch constant (PC) signature.

  // Signature Element.
  static const unsigned kDxilSignatureElementNumFields      = 11;
  static const unsigned kDxilSignatureElementID             = 0;  // Unique element ID.
  static const unsigned kDxilSignatureElementName           = 1;  // Element name.
  static const unsigned kDxilSignatureElementType           = 2;  // Element type.
  static const unsigned kDxilSignatureElementSystemValue    = 3;  // Effective system value.
  static const unsigned kDxilSignatureElementIndexVector    = 4;  // Semantic index vector.
  static const unsigned kDxilSignatureElementInterpMode     = 5;  // Interpolation mode.
  static const unsigned kDxilSignatureElementRows           = 6;  // Number of rows.
  static const unsigned kDxilSignatureElementCols           = 7;  // Number of columns.
  static const unsigned kDxilSignatureElementStartRow       = 8;  // Element packing start row.
  static const unsigned kDxilSignatureElementStartCol       = 9;  // Element packing start column.
  static const unsigned kDxilSignatureElementNameValueList  = 10;  // Name-value list for extended properties.

  // Signature Element Extended Properties.
  static const unsigned kDxilSignatureElementOutputStreamTag    = 0;
  static const unsigned kHLSignatureElementGlobalSymbolTag      = 1;
  static const unsigned kDxilSignatureElementDynIdxCompMaskTag  = 2;

  // Resources.
  static const char kDxilResourcesMDName[];
  static const unsigned kDxilNumResourceFields              = 4;
  static const unsigned kDxilResourceSRVs                   = 0;
  static const unsigned kDxilResourceUAVs                   = 1;
  static const unsigned kDxilResourceCBuffers               = 2;
  static const unsigned kDxilResourceSamplers               = 3;

  // ResourceBase.
  static const unsigned kDxilResourceBaseNumFields          = 6;
  static const unsigned kDxilResourceBaseID                 = 0;  // Unique (per type) resource ID.
  static const unsigned kDxilResourceBaseVariable           = 1;  // Resource global variable.
  static const unsigned kDxilResourceBaseName               = 2;  // Original (HLSL) name of the resource.
  static const unsigned kDxilResourceBaseSpaceID            = 3;  // Resource range space ID.
  static const unsigned kDxilResourceBaseLowerBound         = 4;  // Resource range lower bound.
  static const unsigned kDxilResourceBaseRangeSize          = 5;  // Resource range size.

  // SRV-specific.
  static const unsigned kDxilSRVNumFields                   = 9;
  static const unsigned kDxilSRVShape                       = 6;  // SRV shape.
  static const unsigned kDxilSRVSampleCount                 = 7;  // SRV sample count.
  static const unsigned kDxilSRVNameValueList               = 8;  // Name-value list for extended properties.

  // UAV-specific.
  static const unsigned kDxilUAVNumFields                   = 11;
  static const unsigned kDxilUAVShape                       = 6;  // UAV shape.
  static const unsigned kDxilUAVGloballyCoherent            = 7;  // Globally-coherent UAV.
  static const unsigned kDxilUAVCounter                     = 8;  // UAV with a counter.
  static const unsigned kDxilUAVRasterizerOrderedView       = 9;  // UAV that is a ROV.
  static const unsigned kDxilUAVNameValueList               = 10; // Name-value list for extended properties.

  // CBuffer-specific.
  static const unsigned kDxilCBufferNumFields               = 8;
  static const unsigned kDxilCBufferSizeInBytes             = 6;  // CBuffer size in bytes.
  static const unsigned kDxilCBufferNameValueList           = 7;  // Name-value list for extended properties.

  // CBuffer extended properties
  static const unsigned kHLCBufferIsTBufferTag              = 0;  // CBuffer is actually TBuffer, not yet converted to SRV.

  // Sampler-specific.
  static const unsigned kDxilSamplerNumFields               = 8;
  static const unsigned kDxilSamplerType                    = 6;  // Sampler type.
  static const unsigned kDxilSamplerNameValueList           = 7;  // Name-value list for extended properties.

  // Resource extended property tags.
  static const unsigned kDxilTypedBufferElementTypeTag            = 0;
  static const unsigned kDxilStructuredBufferElementStrideTag     = 1;

  // Type system.
  static const char kDxilTypeSystemMDName[];
  static const char kDxilTypeSystemHelperVariablePrefix[];
  static const unsigned kDxilTypeSystemStructTag                  = 0;
  static const unsigned kDxilTypeSystemFunctionTag                = 1;
  static const unsigned kDxilFieldAnnotationSNormTag              = 0;
  static const unsigned kDxilFieldAnnotationUNormTag              = 1;
  static const unsigned kDxilFieldAnnotationMatrixTag             = 2;
  static const unsigned kDxilFieldAnnotationCBufferOffsetTag      = 3;
  static const unsigned kDxilFieldAnnotationSemanticStringTag     = 4;
  static const unsigned kDxilFieldAnnotationInterpolationModeTag  = 5;
  static const unsigned kDxilFieldAnnotationFieldNameTag          = 6;
  static const unsigned kDxilFieldAnnotationCompTypeTag           = 7;
  static const unsigned kDxilFieldAnnotationPreciseTag            = 8;

  // Control flow hint.
  static const char kDxilControlFlowHintMDName[];

  // Resource attribute.
  static const char kHLDxilResourceAttributeMDName[];
  static const unsigned kHLDxilResourceAttributeNumFields = 2;
  static const unsigned kHLDxilResourceAttributeClass = 0;
  static const unsigned kHLDxilResourceAttributeMeta = 1;

  // Precise attribute.
  static const char kDxilPreciseAttributeMDName[];

  // NonUniform attribute.
  static const char kDxilNonUniformAttributeMDName[];

  // Validator version.
  static const char kDxilValidatorVersionMDName[];
  // Validator version uses the same constants for fields as kDxilVersion*

  // Extended shader property tags.
  static const unsigned kDxilShaderFlagsTag     = 0;
  static const unsigned kDxilGSStateTag         = 1;
  static const unsigned kDxilDSStateTag         = 2;
  static const unsigned kDxilHSStateTag         = 3;
  static const unsigned kDxilNumThreadsTag      = 4;
  static const unsigned kDxilAutoBindingSpaceTag    = 5;
  static const unsigned kDxilRayPayloadSizeTag  = 6;
  static const unsigned kDxilRayAttribSizeTag   = 7;
  static const unsigned kDxilShaderKindTag      = 8;

  // GSState.
  static const unsigned kDxilGSStateNumFields               = 5;
  static const unsigned kDxilGSStateInputPrimitive          = 0;
  static const unsigned kDxilGSStateMaxVertexCount          = 1;
  static const unsigned kDxilGSStateActiveStreamMask        = 2;
  static const unsigned kDxilGSStateOutputStreamTopology    = 3;
  static const unsigned kDxilGSStateGSInstanceCount         = 4;

  // DSState.
  static const unsigned kDxilDSStateNumFields               = 2;
  static const unsigned kDxilDSStateTessellatorDomain       = 0;
  static const unsigned kDxilDSStateInputControlPointCount  = 1;

  // HSState.
  static const unsigned kDxilHSStateNumFields                 = 7;
  static const unsigned kDxilHSStatePatchConstantFunction     = 0;
  static const unsigned kDxilHSStateInputControlPointCount    = 1;
  static const unsigned kDxilHSStateOutputControlPointCount   = 2;
  static const unsigned kDxilHSStateTessellatorDomain         = 3;
  static const unsigned kDxilHSStateTessellatorPartitioning   = 4;
  static const unsigned kDxilHSStateTessellatorOutputPrimitive= 5;
  static const unsigned kDxilHSStateMaxTessellationFactor     = 6;

public:
  /// Use this class to manipulate metadata of DXIL or high-level DX IR specific fields in the record.
  class ExtraPropertyHelper {
  public:
    ExtraPropertyHelper(llvm::Module *pModule);
    virtual ~ExtraPropertyHelper() {}

    virtual void EmitSRVProperties(const DxilResource &SRV, std::vector<llvm::Metadata *> &MDVals) = 0;
    virtual void LoadSRVProperties(const llvm::MDOperand &MDO, DxilResource &SRV) = 0;

    virtual void EmitUAVProperties(const DxilResource &UAV, std::vector<llvm::Metadata *> &MDVals) = 0;
    virtual void LoadUAVProperties(const llvm::MDOperand &MDO, DxilResource &UAV) = 0;

    virtual void EmitCBufferProperties(const DxilCBuffer &CB, std::vector<llvm::Metadata *> &MDVals) = 0;
    virtual void LoadCBufferProperties(const llvm::MDOperand &MDO, DxilCBuffer &CB) = 0;

    virtual void EmitSamplerProperties(const DxilSampler &S, std::vector<llvm::Metadata *> &MDVals) = 0;
    virtual void LoadSamplerProperties(const llvm::MDOperand &MDO, DxilSampler &S) = 0;

    virtual void EmitSignatureElementProperties(const DxilSignatureElement &SE, std::vector<llvm::Metadata *> &MDVals) = 0;
    virtual void LoadSignatureElementProperties(const llvm::MDOperand &MDO, DxilSignatureElement &SE) = 0;

  protected:
    llvm::LLVMContext &m_Ctx;
    llvm::Module *m_pModule;
  };

public:
  DxilMDHelper(llvm::Module *pModule, std::unique_ptr<ExtraPropertyHelper> EPH);
  ~DxilMDHelper();

  void SetShaderModel(const ShaderModel *pSM);
  const ShaderModel *GetShaderModel() const;

  // Dxil version.
  void EmitDxilVersion(unsigned Major, unsigned Minor);
  void LoadDxilVersion(unsigned &Major, unsigned &Minor);

  // Validator version.
  void EmitValidatorVersion(unsigned Major, unsigned Minor);
  void LoadValidatorVersion(unsigned &Major, unsigned &Minor);

  // Shader model.
  void EmitDxilShaderModel(const ShaderModel *pSM);
  void LoadDxilShaderModel(const ShaderModel *&pSM);

  // Intermediate flags
  void EmitDxilIntermediateOptions(uint32_t flags);
  void LoadDxilIntermediateOptions(uint32_t &flags);

  // Entry points.
  void EmitDxilEntryPoints(std::vector<llvm::MDNode *> &MDEntries);
  void UpdateDxilEntryPoints(std::vector<llvm::MDNode *> &MDEntries);
  const llvm::NamedMDNode *GetDxilEntryPoints();
  llvm::MDTuple *EmitDxilEntryPointTuple(llvm::Function *pFunc, const std::string &Name, llvm::MDTuple *pSignatures,
                                         llvm::MDTuple *pResources, llvm::MDTuple *pProperties);
  void GetDxilEntryPoint(const llvm::MDNode *MDO, llvm::Function *&pFunc, std::string &Name,
                         const llvm::MDOperand *&pSignatures, const llvm::MDOperand *&pResources,
                         const llvm::MDOperand *&pProperties);

  // Signatures.
  llvm::MDTuple *EmitDxilSignatures(const DxilEntrySignature &EntrySig);
  void LoadDxilSignatures(const llvm::MDOperand &MDO,
                          DxilEntrySignature &EntrySig);
  llvm::MDTuple *EmitSignatureMetadata(const DxilSignature &Sig);
  void EmitRootSignature(std::vector<uint8_t> &SerializedRootSignature);
  void LoadSignatureMetadata(const llvm::MDOperand &MDO, DxilSignature &Sig);
  llvm::MDTuple *EmitSignatureElement(const DxilSignatureElement &SE);
  void LoadSignatureElement(const llvm::MDOperand &MDO, DxilSignatureElement &SE);
  void LoadRootSignature(std::vector<uint8_t> &SerializedRootSignature);

  // Resources.
  llvm::MDTuple *EmitDxilResourceTuple(llvm::MDTuple *pSRVs, llvm::MDTuple *pUAVs, 
                                       llvm::MDTuple *pCBuffers, llvm::MDTuple *pSamplers);
  void EmitDxilResources(llvm::MDTuple *pDxilResourceTuple);
  void UpdateDxilResources(llvm::MDTuple *pDxilResourceTuple);
  void GetDxilResources(const llvm::MDOperand &MDO, const llvm::MDTuple *&pSRVs, const llvm::MDTuple *&pUAVs, 
                        const llvm::MDTuple *&pCBuffers, const llvm::MDTuple *&pSamplers);
  void EmitDxilResourceBase(const DxilResourceBase &R, llvm::Metadata *ppMDVals[]);
  void LoadDxilResourceBase(const llvm::MDOperand &MDO, DxilResourceBase &R);
  llvm::MDTuple *EmitDxilSRV(const DxilResource &SRV);
  void LoadDxilSRV(const llvm::MDOperand &MDO, DxilResource &SRV);
  llvm::MDTuple *EmitDxilUAV(const DxilResource &UAV);
  void LoadDxilUAV(const llvm::MDOperand &MDO, DxilResource &UAV);
  llvm::MDTuple *EmitDxilCBuffer(const DxilCBuffer &CB);
  void LoadDxilCBuffer(const llvm::MDOperand &MDO, DxilCBuffer &CB);
  llvm::MDTuple *EmitDxilSampler(const DxilSampler &S);
  void LoadDxilSampler(const llvm::MDOperand &MDO, DxilSampler &S);
  const llvm::MDOperand &GetResourceClass(llvm::MDNode *MD, DXIL::ResourceClass &RC);
  void LoadDxilResourceBaseFromMDNode(llvm::MDNode *MD, DxilResourceBase &R);
  void LoadDxilResourceFromMDNode(llvm::MDNode *MD, DxilResource &R);
  void LoadDxilSamplerFromMDNode(llvm::MDNode *MD, DxilSampler &S);

  // Type system.
  void EmitDxilTypeSystem(DxilTypeSystem &TypeSystem, std::vector<llvm::GlobalVariable *> &LLVMUsed);
  void LoadDxilTypeSystemNode(const llvm::MDTuple &MDT, DxilTypeSystem &TypeSystem);
  void LoadDxilTypeSystem(DxilTypeSystem &TypeSystem);
  llvm::Metadata *EmitDxilStructAnnotation(const DxilStructAnnotation &SA);
  void LoadDxilStructAnnotation(const llvm::MDOperand &MDO, DxilStructAnnotation &SA);
  llvm::Metadata *EmitDxilFieldAnnotation(const DxilFieldAnnotation &FA);
  void LoadDxilFieldAnnotation(const llvm::MDOperand &MDO, DxilFieldAnnotation &FA);
  llvm::Metadata *EmitDxilFunctionAnnotation(const DxilFunctionAnnotation &FA);
  void LoadDxilFunctionAnnotation(const llvm::MDOperand &MDO, DxilFunctionAnnotation &FA);
  llvm::Metadata *EmitDxilParamAnnotation(const DxilParameterAnnotation &PA);
  void LoadDxilParamAnnotation(const llvm::MDOperand &MDO, DxilParameterAnnotation &PA);
  llvm::Metadata *EmitDxilParamAnnotations(const DxilFunctionAnnotation &FA);
  void LoadDxilParamAnnotations(const llvm::MDOperand &MDO, DxilFunctionAnnotation &FA);

  // Function props.
  llvm::MDTuple *EmitDxilFunctionProps(const hlsl::DxilFunctionProps *props,
                                       const llvm::Function *F);
  const llvm::Function *LoadDxilFunctionProps(const llvm::MDTuple *pProps,
                                              hlsl::DxilFunctionProps *props);
  llvm::MDTuple *EmitDxilEntryProperties(uint64_t rawShaderFlag,
                                          const hlsl::DxilFunctionProps &props,
                                          uint32_t autoBindingSpace);
  void LoadDxilEntryProperties(const llvm::MDOperand &MDO,
                                uint64_t &rawShaderFlag,
                                hlsl::DxilFunctionProps &props,
                                uint32_t &autoBindingSpace);

  // ViewId state.
  void EmitDxilViewIdState(std::vector<unsigned> &SerializedState);
  void LoadDxilViewIdState(std::vector<unsigned> &SerializedState);
  // Control flow hints.
  static llvm::MDNode *EmitControlFlowHints(llvm::LLVMContext &Ctx, std::vector<DXIL::ControlFlowHint> &hints);
  static unsigned GetControlFlowHintMask(const llvm::Instruction *I);
  static bool HasControlFlowHintToPreventFlatten(const llvm::Instruction *I);

  // Subobjects
  void EmitSubobjects(const DxilSubobjects &Subobjects);
  void LoadSubobjects(DxilSubobjects &Subobjects);
  llvm::Metadata *EmitSubobject(const DxilSubobject &obj);
  void LoadSubobject(const llvm::MDNode &MDO, DxilSubobjects &Subobjects);

  // Shader specific.
private:
  llvm::MDTuple *EmitDxilGSState(DXIL::InputPrimitive Primitive, unsigned MaxVertexCount, 
                                 unsigned ActiveStreamMask, DXIL::PrimitiveTopology StreamPrimitiveTopology,
                                 unsigned GSInstanceCount);
  void LoadDxilGSState(const llvm::MDOperand &MDO, DXIL::InputPrimitive &Primitive, unsigned &MaxVertexCount, 
                       unsigned &ActiveStreamMask, DXIL::PrimitiveTopology &StreamPrimitiveTopology,
                       unsigned &GSInstanceCount);

  llvm::MDTuple *EmitDxilDSState(DXIL::TessellatorDomain Domain, unsigned InputControlPointCount);
  void LoadDxilDSState(const llvm::MDOperand &MDO, DXIL::TessellatorDomain &Domain, unsigned &InputControlPointCount);

  llvm::MDTuple *EmitDxilHSState(llvm::Function *pPatchConstantFunction,
                                 unsigned InputControlPointCount,
                                 unsigned OutputControlPointCount,
                                 DXIL::TessellatorDomain TessDomain,
                                 DXIL::TessellatorPartitioning TessPartitioning,
                                 DXIL::TessellatorOutputPrimitive TessOutputPrimitive,
                                 float MaxTessFactor);
  void LoadDxilHSState(const llvm::MDOperand &MDO,
                       llvm::Function *&pPatchConstantFunction,
                       unsigned &InputControlPointCount,
                       unsigned &OutputControlPointCount,
                       DXIL::TessellatorDomain &TessDomain,
                       DXIL::TessellatorPartitioning &TessPartitioning,
                       DXIL::TessellatorOutputPrimitive &TessOutputPrimitive,
                       float &MaxTessFactor);
public:
  // Utility functions.
  static bool IsKnownNamedMetaData(const llvm::NamedMDNode &Node);
  static void combineDxilMetadata(llvm::Instruction *K, const llvm::Instruction *J);
  static llvm::ConstantAsMetadata *Int32ToConstMD(int32_t v, llvm::LLVMContext &Ctx);
  llvm::ConstantAsMetadata *Int32ToConstMD(int32_t v);
  static llvm::ConstantAsMetadata *Uint32ToConstMD(unsigned v, llvm::LLVMContext &Ctx);
  llvm::ConstantAsMetadata *Uint32ToConstMD(unsigned v);
  static llvm::ConstantAsMetadata *Uint64ToConstMD(uint64_t v, llvm::LLVMContext &Ctx);
  llvm::ConstantAsMetadata *Uint64ToConstMD(uint64_t v);
  llvm::ConstantAsMetadata *Int8ToConstMD(int8_t v);
  llvm::ConstantAsMetadata *Uint8ToConstMD(uint8_t v);
  static llvm::ConstantAsMetadata *BoolToConstMD(bool v, llvm::LLVMContext &Ctx);
  llvm::ConstantAsMetadata *BoolToConstMD(bool v);
  llvm::ConstantAsMetadata *FloatToConstMD(float v);
  static int32_t ConstMDToInt32(const llvm::MDOperand &MDO);
  static unsigned ConstMDToUint32(const llvm::MDOperand &MDO);
  static uint64_t ConstMDToUint64(const llvm::MDOperand &MDO);
  static int8_t ConstMDToInt8(const llvm::MDOperand &MDO);
  static uint8_t ConstMDToUint8(const llvm::MDOperand &MDO);
  static bool ConstMDToBool(const llvm::MDOperand &MDO);
  static float ConstMDToFloat(const llvm::MDOperand &MDO);
  static std::string StringMDToString(const llvm::MDOperand &MDO);
  static llvm::StringRef StringMDToStringRef(const llvm::MDOperand &MDO);
  static llvm::Value *ValueMDToValue(const llvm::MDOperand &MDO);
  llvm::MDTuple *Uint32VectorToConstMDTuple(const std::vector<unsigned> &Vec);
  void ConstMDTupleToUint32Vector(llvm::MDTuple *pTupleMD, std::vector<unsigned> &Vec);
  static bool IsMarkedPrecise(const llvm::Instruction *inst);
  static void MarkPrecise(llvm::Instruction *inst);
  static bool IsMarkedNonUniform(const llvm::Instruction *inst);
  static void MarkNonUniform(llvm::Instruction *inst);

private:
  llvm::LLVMContext &m_Ctx;
  llvm::Module *m_pModule;
  const ShaderModel *m_pSM;
  std::unique_ptr<ExtraPropertyHelper> m_ExtraPropertyHelper;
};


/// Use this class to manipulate metadata of extra metadata record properties that are specific to DXIL.
class DxilExtraPropertyHelper : public DxilMDHelper::ExtraPropertyHelper {
public:
  DxilExtraPropertyHelper(llvm::Module *pModule);
  virtual ~DxilExtraPropertyHelper() {}

  virtual void EmitSRVProperties(const DxilResource &SRV, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadSRVProperties(const llvm::MDOperand &MDO, DxilResource &SRV);

  virtual void EmitUAVProperties(const DxilResource &UAV, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadUAVProperties(const llvm::MDOperand &MDO, DxilResource &UAV);

  virtual void EmitCBufferProperties(const DxilCBuffer &CB, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadCBufferProperties(const llvm::MDOperand &MDO, DxilCBuffer &CB);

  virtual void EmitSamplerProperties(const DxilSampler &S, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadSamplerProperties(const llvm::MDOperand &MDO, DxilSampler &S);

  virtual void EmitSignatureElementProperties(const DxilSignatureElement &SE, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadSignatureElementProperties(const llvm::MDOperand &MDO, DxilSignatureElement &SE);
};

} // namespace hlsl
