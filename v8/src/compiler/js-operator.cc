// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"

#include <limits>

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/objects-inl.h"  // TODO(mstarzinger): Temporary cycle breaker!
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

VectorSlotPair::VectorSlotPair() {}


int VectorSlotPair::index() const {
  return vector_.is_null() ? -1 : vector_->GetIndex(slot_);
}


bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return lhs.slot() == rhs.slot() &&
         lhs.vector().location() == rhs.vector().location();
}


bool operator!=(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(VectorSlotPair const& p) {
  return base::hash_combine(p.slot(), p.vector().location());
}


size_t hash_value(ConvertReceiverMode const& mode) {
  return base::hash_value(static_cast<int>(mode));
}


std::ostream& operator<<(std::ostream& os, ConvertReceiverMode const& mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return os << "NULL_OR_UNDEFINED";
    case ConvertReceiverMode::kNotNullOrUndefined:
      return os << "NOT_NULL_OR_UNDEFINED";
    case ConvertReceiverMode::kAny:
      return os << "ANY";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os, CallFunctionParameters const& p) {
  os << p.arity() << ", " << p.flags() << ", " << p.language_mode();
  if (p.AllowTailCalls()) {
    os << ", ALLOW_TAIL_CALLS";
  }
  return os;
}


const CallFunctionParameters& CallFunctionParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, op->opcode());
  return OpParameter<CallFunctionParameters>(op);
}


bool operator==(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return lhs.id() == rhs.id() && lhs.arity() == rhs.arity();
}


bool operator!=(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CallRuntimeParameters const& p) {
  return base::hash_combine(p.id(), p.arity());
}


std::ostream& operator<<(std::ostream& os, CallRuntimeParameters const& p) {
  return os << p.id() << ", " << p.arity();
}


const CallRuntimeParameters& CallRuntimeParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCallRuntime, op->opcode());
  return OpParameter<CallRuntimeParameters>(op);
}


ContextAccess::ContextAccess(size_t depth, size_t index, bool immutable)
    : immutable_(immutable),
      depth_(static_cast<uint16_t>(depth)),
      index_(static_cast<uint32_t>(index)) {
  DCHECK(depth <= std::numeric_limits<uint16_t>::max());
  DCHECK(index <= std::numeric_limits<uint32_t>::max());
}


bool operator==(ContextAccess const& lhs, ContextAccess const& rhs) {
  return lhs.depth() == rhs.depth() && lhs.index() == rhs.index() &&
         lhs.immutable() == rhs.immutable();
}


bool operator!=(ContextAccess const& lhs, ContextAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(ContextAccess const& access) {
  return base::hash_combine(access.depth(), access.index(), access.immutable());
}


std::ostream& operator<<(std::ostream& os, ContextAccess const& access) {
  return os << access.depth() << ", " << access.index() << ", "
            << access.immutable();
}


ContextAccess const& ContextAccessOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadContext ||
         op->opcode() == IrOpcode::kJSStoreContext);
  return OpParameter<ContextAccess>(op);
}


DynamicGlobalAccess::DynamicGlobalAccess(const Handle<String>& name,
                                         uint32_t check_bitset,
                                         const VectorSlotPair& feedback,
                                         TypeofMode typeof_mode)
    : name_(name),
      check_bitset_(check_bitset),
      feedback_(feedback),
      typeof_mode_(typeof_mode) {
  DCHECK(check_bitset == kFullCheckRequired || check_bitset < 0x80000000U);
}


bool operator==(DynamicGlobalAccess const& lhs,
                DynamicGlobalAccess const& rhs) {
  UNIMPLEMENTED();
  return true;
}


bool operator!=(DynamicGlobalAccess const& lhs,
                DynamicGlobalAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(DynamicGlobalAccess const& access) {
  UNIMPLEMENTED();
  return 0;
}


std::ostream& operator<<(std::ostream& os, DynamicGlobalAccess const& access) {
  return os << Brief(*access.name()) << ", " << access.check_bitset() << ", "
            << access.typeof_mode();
}


DynamicGlobalAccess const& DynamicGlobalAccessOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSLoadDynamicGlobal, op->opcode());
  return OpParameter<DynamicGlobalAccess>(op);
}


DynamicContextAccess::DynamicContextAccess(const Handle<String>& name,
                                           uint32_t check_bitset,
                                           const ContextAccess& context_access)
    : name_(name),
      check_bitset_(check_bitset),
      context_access_(context_access) {
  DCHECK(check_bitset == kFullCheckRequired || check_bitset < 0x80000000U);
}


bool operator==(DynamicContextAccess const& lhs,
                DynamicContextAccess const& rhs) {
  UNIMPLEMENTED();
  return true;
}


bool operator!=(DynamicContextAccess const& lhs,
                DynamicContextAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(DynamicContextAccess const& access) {
  UNIMPLEMENTED();
  return 0;
}


std::ostream& operator<<(std::ostream& os, DynamicContextAccess const& access) {
  return os << Brief(*access.name()) << ", " << access.check_bitset() << ", "
            << access.context_access();
}


DynamicContextAccess const& DynamicContextAccessOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSLoadDynamicContext, op->opcode());
  return OpParameter<DynamicContextAccess>(op);
}


bool operator==(NamedAccess const& lhs, NamedAccess const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(NamedAccess const& lhs, NamedAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(NamedAccess const& p) {
  return base::hash_combine(p.name().location(), p.language_mode(),
                            p.feedback());
}


std::ostream& operator<<(std::ostream& os, NamedAccess const& p) {
  return os << Brief(*p.name()) << ", " << p.language_mode();
}


NamedAccess const& NamedAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadNamed ||
         op->opcode() == IrOpcode::kJSStoreNamed);
  return OpParameter<NamedAccess>(op);
}


std::ostream& operator<<(std::ostream& os, PropertyAccess const& p) {
  return os << p.language_mode();
}


bool operator==(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return !(lhs == rhs);
}


PropertyAccess const& PropertyAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadProperty ||
         op->opcode() == IrOpcode::kJSStoreProperty);
  return OpParameter<PropertyAccess>(op);
}


size_t hash_value(PropertyAccess const& p) {
  return base::hash_combine(p.language_mode(), p.feedback());
}


bool operator==(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback() &&
         lhs.typeof_mode() == rhs.typeof_mode();
}


bool operator!=(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(LoadGlobalParameters const& p) {
  return base::hash_combine(p.name().location(), p.typeof_mode());
}


std::ostream& operator<<(std::ostream& os, LoadGlobalParameters const& p) {
  return os << Brief(*p.name()) << ", " << p.typeof_mode();
}


const LoadGlobalParameters& LoadGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, op->opcode());
  return OpParameter<LoadGlobalParameters>(op);
}


bool operator==(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreGlobalParameters const& p) {
  return base::hash_combine(p.language_mode(), p.name().location(),
                            p.feedback());
}


std::ostream& operator<<(std::ostream& os, StoreGlobalParameters const& p) {
  return os << p.language_mode() << ", " << Brief(*p.name());
}


const StoreGlobalParameters& StoreGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreGlobal, op->opcode());
  return OpParameter<StoreGlobalParameters>(op);
}


bool operator==(CreateArgumentsParameters const& lhs,
                CreateArgumentsParameters const& rhs) {
  return lhs.type() == rhs.type() && lhs.start_index() == rhs.start_index();
}


bool operator!=(CreateArgumentsParameters const& lhs,
                CreateArgumentsParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateArgumentsParameters const& p) {
  return base::hash_combine(p.type(), p.start_index());
}


std::ostream& operator<<(std::ostream& os, CreateArgumentsParameters const& p) {
  return os << p.type() << ", " << p.start_index();
}


const CreateArgumentsParameters& CreateArgumentsParametersOf(
    const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, op->opcode());
  return OpParameter<CreateArgumentsParameters>(op);
}


bool operator==(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return lhs.pretenure() == rhs.pretenure() &&
         lhs.shared_info().location() == rhs.shared_info().location();
}


bool operator!=(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateClosureParameters const& p) {
  return base::hash_combine(p.pretenure(), p.shared_info().location());
}


std::ostream& operator<<(std::ostream& os, CreateClosureParameters const& p) {
  return os << p.pretenure() << ", " << Brief(*p.shared_info());
}


const CreateClosureParameters& CreateClosureParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, op->opcode());
  return OpParameter<CreateClosureParameters>(op);
}


#define CACHED_OP_LIST(V)                                 \
  V(Equal, Operator::kNoProperties, 2, 1)                 \
  V(NotEqual, Operator::kNoProperties, 2, 1)              \
  V(StrictEqual, Operator::kNoThrow, 2, 1)                \
  V(StrictNotEqual, Operator::kNoThrow, 2, 1)             \
  V(UnaryNot, Operator::kEliminatable, 1, 1)              \
  V(ToBoolean, Operator::kEliminatable, 1, 1)             \
  V(ToNumber, Operator::kNoProperties, 1, 1)              \
  V(ToString, Operator::kNoProperties, 1, 1)              \
  V(ToName, Operator::kNoProperties, 1, 1)                \
  V(ToObject, Operator::kNoProperties, 1, 1)              \
  V(Yield, Operator::kNoProperties, 1, 1)                 \
  V(Create, Operator::kEliminatable, 0, 1)                \
  V(HasProperty, Operator::kNoProperties, 2, 1)           \
  V(TypeOf, Operator::kEliminatable, 1, 1)                \
  V(InstanceOf, Operator::kNoProperties, 2, 1)            \
  V(ForInDone, Operator::kPure, 2, 1)                     \
  V(ForInNext, Operator::kNoProperties, 4, 1)             \
  V(ForInPrepare, Operator::kNoProperties, 1, 3)          \
  V(ForInStep, Operator::kPure, 1, 1)                     \
  V(StackCheck, Operator::kNoProperties, 0, 0)            \
  V(CreateWithContext, Operator::kNoProperties, 2, 1)     \
  V(CreateModuleContext, Operator::kNoProperties, 2, 1)


#define CACHED_OP_LIST_WITH_LANGUAGE_MODE(V)           \
  V(LessThan, Operator::kNoProperties, 2, 1)           \
  V(GreaterThan, Operator::kNoProperties, 2, 1)        \
  V(LessThanOrEqual, Operator::kNoProperties, 2, 1)    \
  V(GreaterThanOrEqual, Operator::kNoProperties, 2, 1) \
  V(BitwiseOr, Operator::kNoProperties, 2, 1)          \
  V(BitwiseXor, Operator::kNoProperties, 2, 1)         \
  V(BitwiseAnd, Operator::kNoProperties, 2, 1)         \
  V(ShiftLeft, Operator::kNoProperties, 2, 1)          \
  V(ShiftRight, Operator::kNoProperties, 2, 1)         \
  V(ShiftRightLogical, Operator::kNoProperties, 2, 1)  \
  V(Add, Operator::kNoProperties, 2, 1)                \
  V(Subtract, Operator::kNoProperties, 2, 1)           \
  V(Multiply, Operator::kNoProperties, 2, 1)           \
  V(Divide, Operator::kNoProperties, 2, 1)             \
  V(Modulus, Operator::kNoProperties, 2, 1)


struct JSOperatorGlobalCache final {
#define CACHED(Name, properties, value_input_count, value_output_count)  \
  struct Name##Operator final : public Operator {                        \
    Name##Operator()                                                     \
        : Operator(IrOpcode::kJS##Name, properties, "JS" #Name,          \
                   value_input_count, Operator::ZeroIfPure(properties),  \
                   Operator::ZeroIfEliminatable(properties),             \
                   value_output_count, Operator::ZeroIfPure(properties), \
                   Operator::ZeroIfNoThrow(properties)) {}               \
  };                                                                     \
  Name##Operator k##Name##Operator;
  CACHED_OP_LIST(CACHED)
#undef CACHED


#define CACHED_WITH_LANGUAGE_MODE(Name, properties, value_input_count,        \
                                  value_output_count)                         \
  template <LanguageMode kLanguageMode>                                       \
  struct Name##Operator final : public Operator1<LanguageMode> {              \
    Name##Operator()                                                          \
        : Operator1<LanguageMode>(                                            \
              IrOpcode::kJS##Name, properties, "JS" #Name, value_input_count, \
              Operator::ZeroIfPure(properties),                               \
              Operator::ZeroIfEliminatable(properties), value_output_count,   \
              Operator::ZeroIfPure(properties),                               \
              Operator::ZeroIfNoThrow(properties), kLanguageMode) {}          \
  };                                                                          \
  Name##Operator<SLOPPY> k##Name##SloppyOperator;                             \
  Name##Operator<STRICT> k##Name##StrictOperator;                             \
  Name##Operator<STRONG> k##Name##StrongOperator;
  CACHED_OP_LIST_WITH_LANGUAGE_MODE(CACHED_WITH_LANGUAGE_MODE)
#undef CACHED_WITH_LANGUAGE_MODE
};


static base::LazyInstance<JSOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;


JSOperatorBuilder::JSOperatorBuilder(Zone* zone)
    : cache_(kCache.Get()), zone_(zone) {}


#define CACHED(Name, properties, value_input_count, value_output_count) \
  const Operator* JSOperatorBuilder::Name() {                           \
    return &cache_.k##Name##Operator;                                   \
  }
CACHED_OP_LIST(CACHED)
#undef CACHED


#define CACHED_WITH_LANGUAGE_MODE(Name, properties, value_input_count,  \
                                  value_output_count)                   \
  const Operator* JSOperatorBuilder::Name(LanguageMode language_mode) { \
    switch (language_mode) {                                            \
      case SLOPPY:                                                      \
        return &cache_.k##Name##SloppyOperator;                         \
      case STRICT:                                                      \
        return &cache_.k##Name##StrictOperator;                         \
      case STRONG:                                                      \
        return &cache_.k##Name##StrongOperator;                         \
      default:                                                          \
        break; /* %*!%^$#@ */                                           \
    }                                                                   \
    UNREACHABLE();                                                      \
    return nullptr;                                                     \
  }
CACHED_OP_LIST_WITH_LANGUAGE_MODE(CACHED_WITH_LANGUAGE_MODE)
#undef CACHED_WITH_LANGUAGE_MODE


const Operator* JSOperatorBuilder::CallFunction(
    size_t arity, CallFunctionFlags flags, LanguageMode language_mode,
    VectorSlotPair const& feedback, ConvertReceiverMode convert_mode,
    TailCallMode tail_call_mode) {
  CallFunctionParameters parameters(arity, flags, language_mode, feedback,
                                    tail_call_mode, convert_mode);
  return new (zone()) Operator1<CallFunctionParameters>(   // --
      IrOpcode::kJSCallFunction, Operator::kNoProperties,  // opcode
      "JSCallFunction",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                   // inputs/outputs
      parameters);                                         // parameter
}


const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id,
                                               size_t arity) {
  CallRuntimeParameters parameters(id, arity);
  const Runtime::Function* f = Runtime::FunctionForId(parameters.id());
  DCHECK(f->nargs == -1 || f->nargs == static_cast<int>(parameters.arity()));
  return new (zone()) Operator1<CallRuntimeParameters>(   // --
      IrOpcode::kJSCallRuntime, Operator::kNoProperties,  // opcode
      "JSCallRuntime",                                    // name
      parameters.arity(), 1, 1, f->result_size, 1, 2,     // inputs/outputs
      parameters);                                        // parameter
}


const Operator* JSOperatorBuilder::CallConstruct(int arguments) {
  return new (zone()) Operator1<int>(                       // --
      IrOpcode::kJSCallConstruct, Operator::kNoProperties,  // opcode
      "JSCallConstruct",                                    // name
      arguments, 1, 1, 1, 1, 2,                             // counts
      arguments);                                           // parameter
}


const Operator* JSOperatorBuilder::ConvertReceiver(
    ConvertReceiverMode convert_mode) {
  return new (zone()) Operator1<ConvertReceiverMode>(    // --
      IrOpcode::kJSConvertReceiver, Operator::kNoThrow,  // opcode
      "JSConvertReceiver",                               // name
      1, 1, 1, 1, 1, 0,                                  // counts
      convert_mode);                                     // parameter
}


const Operator* JSOperatorBuilder::LoadNamed(LanguageMode language_mode,
                                             Handle<Name> name,
                                             const VectorSlotPair& feedback) {
  NamedAccess access(language_mode, name, feedback);
  return new (zone()) Operator1<NamedAccess>(           // --
      IrOpcode::kJSLoadNamed, Operator::kNoProperties,  // opcode
      "JSLoadNamed",                                    // name
      2, 1, 1, 1, 1, 2,                                 // counts
      access);                                          // parameter
}


const Operator* JSOperatorBuilder::LoadProperty(
    LanguageMode language_mode, VectorSlotPair const& feedback) {
  PropertyAccess access(language_mode, feedback);
  return new (zone()) Operator1<PropertyAccess>(           // --
      IrOpcode::kJSLoadProperty, Operator::kNoProperties,  // opcode
      "JSLoadProperty",                                    // name
      3, 1, 1, 1, 1, 2,                                    // counts
      access);                                             // parameter
}


const Operator* JSOperatorBuilder::StoreNamed(LanguageMode language_mode,
                                              Handle<Name> name,
                                              VectorSlotPair const& feedback) {
  NamedAccess access(language_mode, name, feedback);
  return new (zone()) Operator1<NamedAccess>(            // --
      IrOpcode::kJSStoreNamed, Operator::kNoProperties,  // opcode
      "JSStoreNamed",                                    // name
      3, 1, 1, 0, 1, 2,                                  // counts
      access);                                           // parameter
}


const Operator* JSOperatorBuilder::StoreProperty(
    LanguageMode language_mode, VectorSlotPair const& feedback) {
  PropertyAccess access(language_mode, feedback);
  return new (zone()) Operator1<PropertyAccess>(            // --
      IrOpcode::kJSStoreProperty, Operator::kNoProperties,  // opcode
      "JSStoreProperty",                                    // name
      4, 1, 1, 0, 1, 2,                                     // counts
      access);                                              // parameter
}


const Operator* JSOperatorBuilder::DeleteProperty(LanguageMode language_mode) {
  return new (zone()) Operator1<LanguageMode>(               // --
      IrOpcode::kJSDeleteProperty, Operator::kNoProperties,  // opcode
      "JSDeleteProperty",                                    // name
      2, 1, 1, 1, 1, 2,                                      // counts
      language_mode);                                        // parameter
}


const Operator* JSOperatorBuilder::LoadGlobal(const Handle<Name>& name,
                                              const VectorSlotPair& feedback,
                                              TypeofMode typeof_mode) {
  LoadGlobalParameters parameters(name, feedback, typeof_mode);
  return new (zone()) Operator1<LoadGlobalParameters>(   // --
      IrOpcode::kJSLoadGlobal, Operator::kNoProperties,  // opcode
      "JSLoadGlobal",                                    // name
      1, 1, 1, 1, 1, 2,                                  // counts
      parameters);                                       // parameter
}


const Operator* JSOperatorBuilder::StoreGlobal(LanguageMode language_mode,
                                               const Handle<Name>& name,
                                               const VectorSlotPair& feedback) {
  StoreGlobalParameters parameters(language_mode, feedback, name);
  return new (zone()) Operator1<StoreGlobalParameters>(   // --
      IrOpcode::kJSStoreGlobal, Operator::kNoProperties,  // opcode
      "JSStoreGlobal",                                    // name
      2, 1, 1, 0, 1, 2,                                   // counts
      parameters);                                        // parameter
}


const Operator* JSOperatorBuilder::LoadContext(size_t depth, size_t index,
                                               bool immutable) {
  ContextAccess access(depth, index, immutable);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSLoadContext,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,   // flags
      "JSLoadContext",                           // name
      1, 1, 0, 1, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::StoreContext(size_t depth, size_t index) {
  ContextAccess access(depth, index, false);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSStoreContext,                 // opcode
      Operator::kNoRead | Operator::kNoThrow,    // flags
      "JSStoreContext",                          // name
      2, 1, 1, 0, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::LoadDynamicGlobal(
    const Handle<String>& name, uint32_t check_bitset,
    const VectorSlotPair& feedback, TypeofMode typeof_mode) {
  DynamicGlobalAccess access(name, check_bitset, feedback, typeof_mode);
  return new (zone()) Operator1<DynamicGlobalAccess>(           // --
      IrOpcode::kJSLoadDynamicGlobal, Operator::kNoProperties,  // opcode
      "JSLoadDynamicGlobal",                                    // name
      2, 1, 1, 1, 1, 2,                                         // counts
      access);                                                  // parameter
}


const Operator* JSOperatorBuilder::LoadDynamicContext(
    const Handle<String>& name, uint32_t check_bitset, size_t depth,
    size_t index) {
  ContextAccess context_access(depth, index, false);
  DynamicContextAccess access(name, check_bitset, context_access);
  return new (zone()) Operator1<DynamicContextAccess>(           // --
      IrOpcode::kJSLoadDynamicContext, Operator::kNoProperties,  // opcode
      "JSLoadDynamicContext",                                    // name
      1, 1, 1, 1, 1, 2,                                          // counts
      access);                                                   // parameter
}


const Operator* JSOperatorBuilder::CreateArguments(
    CreateArgumentsParameters::Type type, int start_index) {
  DCHECK_IMPLIES(start_index, type == CreateArgumentsParameters::kRestArray);
  CreateArgumentsParameters parameters(type, start_index);
  return new (zone()) Operator1<CreateArgumentsParameters>(  // --
      IrOpcode::kJSCreateArguments, Operator::kNoThrow,      // opcode
      "JSCreateArguments",                                   // name
      1, 1, 1, 1, 1, 0,                                      // counts
      parameters);                                           // parameter
}


const Operator* JSOperatorBuilder::CreateClosure(
    Handle<SharedFunctionInfo> shared_info, PretenureFlag pretenure) {
  CreateClosureParameters parameters(shared_info, pretenure);
  return new (zone()) Operator1<CreateClosureParameters>(  // --
      IrOpcode::kJSCreateClosure, Operator::kNoThrow,      // opcode
      "JSCreateClosure",                                   // name
      0, 1, 1, 1, 1, 0,                                    // counts
      parameters);                                         // parameter
}


const Operator* JSOperatorBuilder::CreateLiteralArray(int literal_flags) {
  return new (zone()) Operator1<int>(                            // --
      IrOpcode::kJSCreateLiteralArray, Operator::kNoProperties,  // opcode
      "JSCreateLiteralArray",                                    // name
      3, 1, 1, 1, 1, 2,                                          // counts
      literal_flags);                                            // parameter
}


const Operator* JSOperatorBuilder::CreateLiteralObject(int literal_flags) {
  return new (zone()) Operator1<int>(                             // --
      IrOpcode::kJSCreateLiteralObject, Operator::kNoProperties,  // opcode
      "JSCreateLiteralObject",                                    // name
      3, 1, 1, 1, 1, 2,                                           // counts
      literal_flags);                                             // parameter
}


const Operator* JSOperatorBuilder::CreateFunctionContext(int slot_count) {
  return new (zone()) Operator1<int>(                               // --
      IrOpcode::kJSCreateFunctionContext, Operator::kNoProperties,  // opcode
      "JSCreateFunctionContext",                                    // name
      1, 1, 1, 1, 1, 2,                                             // counts
      slot_count);                                                  // parameter
}


const Operator* JSOperatorBuilder::CreateCatchContext(
    const Handle<String>& name) {
  return new (zone()) Operator1<Handle<String>, Handle<String>::equal_to,
                                Handle<String>::hash>(           // --
      IrOpcode::kJSCreateCatchContext, Operator::kNoProperties,  // opcode
      "JSCreateCatchContext",                                    // name
      2, 1, 1, 1, 1, 2,                                          // counts
      name);                                                     // parameter
}


const Operator* JSOperatorBuilder::CreateBlockContext(
    const Handle<ScopeInfo>& scpope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>, Handle<ScopeInfo>::equal_to,
                                Handle<ScopeInfo>::hash>(        // --
      IrOpcode::kJSCreateBlockContext, Operator::kNoProperties,  // opcode
      "JSCreateBlockContext",                                    // name
      1, 1, 1, 1, 1, 2,                                          // counts
      scpope_info);                                              // parameter
}


const Operator* JSOperatorBuilder::CreateScriptContext(
    const Handle<ScopeInfo>& scpope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>, Handle<ScopeInfo>::equal_to,
                                Handle<ScopeInfo>::hash>(         // --
      IrOpcode::kJSCreateScriptContext, Operator::kNoProperties,  // opcode
      "JSCreateScriptContext",                                    // name
      1, 1, 1, 1, 1, 2,                                           // counts
      scpope_info);                                               // parameter
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
