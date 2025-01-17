// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Operator;
struct JSOperatorGlobalCache;


// Defines a pair of {TypeFeedbackVector} and {TypeFeedbackVectorSlot}, which
// is used to access the type feedback for a certain {Node}.
class VectorSlotPair {
 public:
  VectorSlotPair();
  VectorSlotPair(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : vector_(vector), slot_(slot) {}

  bool IsValid() const { return !vector_.is_null() && !slot_.IsInvalid(); }

  Handle<TypeFeedbackVector> vector() const { return vector_; }
  FeedbackVectorSlot slot() const { return slot_; }

  int index() const;

 private:
  const Handle<TypeFeedbackVector> vector_;
  const FeedbackVectorSlot slot_;
};

bool operator==(VectorSlotPair const&, VectorSlotPair const&);
bool operator!=(VectorSlotPair const&, VectorSlotPair const&);

size_t hash_value(VectorSlotPair const&);


// Defines hints about receiver values based on structural knowledge. This is
// used as a parameter by JSConvertReceiver operators.
enum class ConvertReceiverMode {
  kNullOrUndefined,     // Guaranteed to be null or undefined.
  kNotNullOrUndefined,  // Guaranteed to never be null or undefined.
  kAny                  // No specific knowledge about receiver.
};

size_t hash_value(ConvertReceiverMode const&);

std::ostream& operator<<(std::ostream&, ConvertReceiverMode const&);

const ConvertReceiverMode& ConvertReceiverModeOf(const Operator* op);


// Defines whether tail call optimization is allowed.
enum TailCallMode { NO_TAIL_CALLS, ALLOW_TAIL_CALLS };


// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCallFunction operators.
class CallFunctionParameters final {
 public:
  CallFunctionParameters(size_t arity, CallFunctionFlags flags,
                         LanguageMode language_mode,
                         VectorSlotPair const& feedback,
                         TailCallMode tail_call_mode,
                         ConvertReceiverMode convert_mode)
      : bit_field_(ArityField::encode(arity) | FlagsField::encode(flags) |
                   LanguageModeField::encode(language_mode)),
        feedback_(feedback),
        tail_call_mode_(tail_call_mode),
        convert_mode_(convert_mode) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  CallFunctionFlags flags() const { return FlagsField::decode(bit_field_); }
  LanguageMode language_mode() const {
    return LanguageModeField::decode(bit_field_);
  }
  ConvertReceiverMode convert_mode() const { return convert_mode_; }
  VectorSlotPair const& feedback() const { return feedback_; }

  bool operator==(CallFunctionParameters const& that) const {
    return this->bit_field_ == that.bit_field_ &&
           this->feedback_ == that.feedback_ &&
           this->tail_call_mode_ == that.tail_call_mode_ &&
           this->convert_mode_ == that.convert_mode_;
  }
  bool operator!=(CallFunctionParameters const& that) const {
    return !(*this == that);
  }

  bool AllowTailCalls() const { return tail_call_mode_ == ALLOW_TAIL_CALLS; }

 private:
  friend size_t hash_value(CallFunctionParameters const& p) {
    return base::hash_combine(p.bit_field_, p.feedback_, p.convert_mode_);
  }

  typedef BitField<size_t, 0, 28> ArityField;
  typedef BitField<CallFunctionFlags, 28, 2> FlagsField;
  typedef BitField<LanguageMode, 30, 2> LanguageModeField;

  const uint32_t bit_field_;
  const VectorSlotPair feedback_;
  TailCallMode tail_call_mode_;
  ConvertReceiverMode convert_mode_;
};

size_t hash_value(CallFunctionParameters const&);

std::ostream& operator<<(std::ostream&, CallFunctionParameters const&);

const CallFunctionParameters& CallFunctionParametersOf(const Operator* op);


// Defines the arity and the ID for a runtime function call. This is used as a
// parameter by JSCallRuntime operators.
class CallRuntimeParameters final {
 public:
  CallRuntimeParameters(Runtime::FunctionId id, size_t arity)
      : id_(id), arity_(arity) {}

  Runtime::FunctionId id() const { return id_; }
  size_t arity() const { return arity_; }

 private:
  const Runtime::FunctionId id_;
  const size_t arity_;
};

bool operator==(CallRuntimeParameters const&, CallRuntimeParameters const&);
bool operator!=(CallRuntimeParameters const&, CallRuntimeParameters const&);

size_t hash_value(CallRuntimeParameters const&);

std::ostream& operator<<(std::ostream&, CallRuntimeParameters const&);

const CallRuntimeParameters& CallRuntimeParametersOf(const Operator* op);


// Defines the location of a context slot relative to a specific scope. This is
// used as a parameter by JSLoadContext and JSStoreContext operators and allows
// accessing a context-allocated variable without keeping track of the scope.
class ContextAccess final {
 public:
  ContextAccess(size_t depth, size_t index, bool immutable);

  size_t depth() const { return depth_; }
  size_t index() const { return index_; }
  bool immutable() const { return immutable_; }

 private:
  // For space reasons, we keep this tightly packed, otherwise we could just use
  // a simple int/int/bool POD.
  const bool immutable_;
  const uint16_t depth_;
  const uint32_t index_;
};

bool operator==(ContextAccess const&, ContextAccess const&);
bool operator!=(ContextAccess const&, ContextAccess const&);

size_t hash_value(ContextAccess const&);

std::ostream& operator<<(std::ostream&, ContextAccess const&);

ContextAccess const& ContextAccessOf(Operator const*);


// Defines the name for a dynamic variable lookup. The {check_bitset} allows to
// inline checks whether the lookup yields in a global variable. This is used as
// a parameter by JSLoadDynamicGlobal and JSStoreDynamicGlobal operators.
class DynamicGlobalAccess final {
 public:
  DynamicGlobalAccess(const Handle<String>& name, uint32_t check_bitset,
                      const VectorSlotPair& feedback, TypeofMode typeof_mode);

  const Handle<String>& name() const { return name_; }
  uint32_t check_bitset() const { return check_bitset_; }
  const VectorSlotPair& feedback() const { return feedback_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  // Indicates that an inline check is disabled.
  bool RequiresFullCheck() const {
    return check_bitset() == kFullCheckRequired;
  }

  // Limit of context chain length to which inline check is possible.
  static const int kMaxCheckDepth = 30;

  // Sentinel for {check_bitset} disabling inline checks.
  static const uint32_t kFullCheckRequired = -1;

 private:
  const Handle<String> name_;
  const uint32_t check_bitset_;
  const VectorSlotPair feedback_;
  const TypeofMode typeof_mode_;
};

size_t hash_value(DynamicGlobalAccess const&);

bool operator==(DynamicGlobalAccess const&, DynamicGlobalAccess const&);
bool operator!=(DynamicGlobalAccess const&, DynamicGlobalAccess const&);

std::ostream& operator<<(std::ostream&, DynamicGlobalAccess const&);

DynamicGlobalAccess const& DynamicGlobalAccessOf(Operator const*);


// Defines the name for a dynamic variable lookup. The {check_bitset} allows to
// inline checks whether the lookup yields in a context variable. This is used
// as a parameter by JSLoadDynamicContext and JSStoreDynamicContext operators.
class DynamicContextAccess final {
 public:
  DynamicContextAccess(const Handle<String>& name, uint32_t check_bitset,
                       const ContextAccess& context_access);

  const Handle<String>& name() const { return name_; }
  uint32_t check_bitset() const { return check_bitset_; }
  const ContextAccess& context_access() const { return context_access_; }

  // Indicates that an inline check is disabled.
  bool RequiresFullCheck() const {
    return check_bitset() == kFullCheckRequired;
  }

  // Limit of context chain length to which inline check is possible.
  static const int kMaxCheckDepth = 30;

  // Sentinel for {check_bitset} disabling inline checks.
  static const uint32_t kFullCheckRequired = -1;

 private:
  const Handle<String> name_;
  const uint32_t check_bitset_;
  const ContextAccess context_access_;
};

size_t hash_value(DynamicContextAccess const&);

bool operator==(DynamicContextAccess const&, DynamicContextAccess const&);
bool operator!=(DynamicContextAccess const&, DynamicContextAccess const&);

std::ostream& operator<<(std::ostream&, DynamicContextAccess const&);

DynamicContextAccess const& DynamicContextAccessOf(Operator const*);


// Defines the property of an object for a named access. This is
// used as a parameter by the JSLoadNamed and JSStoreNamed operators.
class NamedAccess final {
 public:
  NamedAccess(LanguageMode language_mode, Handle<Name> name,
              VectorSlotPair const& feedback)
      : name_(name), feedback_(feedback), language_mode_(language_mode) {}

  Handle<Name> name() const { return name_; }
  LanguageMode language_mode() const { return language_mode_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  Handle<Name> const name_;
  VectorSlotPair const feedback_;
  LanguageMode const language_mode_;
};

bool operator==(NamedAccess const&, NamedAccess const&);
bool operator!=(NamedAccess const&, NamedAccess const&);

size_t hash_value(NamedAccess const&);

std::ostream& operator<<(std::ostream&, NamedAccess const&);

const NamedAccess& NamedAccessOf(const Operator* op);


// Defines the property being loaded from an object by a named load. This is
// used as a parameter by JSLoadGlobal operator.
class LoadGlobalParameters final {
 public:
  LoadGlobalParameters(const Handle<Name>& name, const VectorSlotPair& feedback,
                       TypeofMode typeof_mode)
      : name_(name), feedback_(feedback), typeof_mode_(typeof_mode) {}

  const Handle<Name>& name() const { return name_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  const Handle<Name> name_;
  const VectorSlotPair feedback_;
  const TypeofMode typeof_mode_;
};

bool operator==(LoadGlobalParameters const&, LoadGlobalParameters const&);
bool operator!=(LoadGlobalParameters const&, LoadGlobalParameters const&);

size_t hash_value(LoadGlobalParameters const&);

std::ostream& operator<<(std::ostream&, LoadGlobalParameters const&);

const LoadGlobalParameters& LoadGlobalParametersOf(const Operator* op);


// Defines the property being stored to an object by a named store. This is
// used as a parameter by JSStoreGlobal operator.
class StoreGlobalParameters final {
 public:
  StoreGlobalParameters(LanguageMode language_mode,
                        const VectorSlotPair& feedback,
                        const Handle<Name>& name)
      : language_mode_(language_mode), name_(name), feedback_(feedback) {}

  LanguageMode language_mode() const { return language_mode_; }
  const VectorSlotPair& feedback() const { return feedback_; }
  const Handle<Name>& name() const { return name_; }

 private:
  const LanguageMode language_mode_;
  const Handle<Name> name_;
  const VectorSlotPair feedback_;
};

bool operator==(StoreGlobalParameters const&, StoreGlobalParameters const&);
bool operator!=(StoreGlobalParameters const&, StoreGlobalParameters const&);

size_t hash_value(StoreGlobalParameters const&);

std::ostream& operator<<(std::ostream&, StoreGlobalParameters const&);

const StoreGlobalParameters& StoreGlobalParametersOf(const Operator* op);


// Defines the property of an object for a keyed access. This is used
// as a parameter by the JSLoadProperty and JSStoreProperty operators.
class PropertyAccess final {
 public:
  PropertyAccess(LanguageMode language_mode, VectorSlotPair const& feedback)
      : feedback_(feedback), language_mode_(language_mode) {}

  LanguageMode language_mode() const { return language_mode_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  VectorSlotPair const feedback_;
  LanguageMode const language_mode_;
};

bool operator==(PropertyAccess const&, PropertyAccess const&);
bool operator!=(PropertyAccess const&, PropertyAccess const&);

size_t hash_value(PropertyAccess const&);

std::ostream& operator<<(std::ostream&, PropertyAccess const&);

PropertyAccess const& PropertyAccessOf(const Operator* op);


// Defines specifics about arguments object or rest parameter creation. This is
// used as a parameter by JSCreateArguments operators.
class CreateArgumentsParameters final {
 public:
  enum Type { kMappedArguments, kUnmappedArguments, kRestArray };
  CreateArgumentsParameters(Type type, int start_index)
      : type_(type), start_index_(start_index) {}

  Type type() const { return type_; }
  int start_index() const { return start_index_; }

 private:
  const Type type_;
  const int start_index_;
};

bool operator==(CreateArgumentsParameters const&,
                CreateArgumentsParameters const&);
bool operator!=(CreateArgumentsParameters const&,
                CreateArgumentsParameters const&);

size_t hash_value(CreateArgumentsParameters const&);

std::ostream& operator<<(std::ostream&, CreateArgumentsParameters const&);

const CreateArgumentsParameters& CreateArgumentsParametersOf(
    const Operator* op);


// Defines shared information for the closure that should be created. This is
// used as a parameter by JSCreateClosure operators.
class CreateClosureParameters final {
 public:
  CreateClosureParameters(Handle<SharedFunctionInfo> shared_info,
                          PretenureFlag pretenure)
      : shared_info_(shared_info), pretenure_(pretenure) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  PretenureFlag pretenure() const { return pretenure_; }

 private:
  const Handle<SharedFunctionInfo> shared_info_;
  const PretenureFlag pretenure_;
};

bool operator==(CreateClosureParameters const&, CreateClosureParameters const&);
bool operator!=(CreateClosureParameters const&, CreateClosureParameters const&);

size_t hash_value(CreateClosureParameters const&);

std::ostream& operator<<(std::ostream&, CreateClosureParameters const&);

const CreateClosureParameters& CreateClosureParametersOf(const Operator* op);


// Interface for building JavaScript-level operators, e.g. directly from the
// AST. Most operators have no parameters, thus can be globally shared for all
// graphs.
class JSOperatorBuilder final : public ZoneObject {
 public:
  explicit JSOperatorBuilder(Zone* zone);

  const Operator* Equal();
  const Operator* NotEqual();
  const Operator* StrictEqual();
  const Operator* StrictNotEqual();
  const Operator* LessThan(LanguageMode language_mode);
  const Operator* GreaterThan(LanguageMode language_mode);
  const Operator* LessThanOrEqual(LanguageMode language_mode);
  const Operator* GreaterThanOrEqual(LanguageMode language_mode);
  const Operator* BitwiseOr(LanguageMode language_mode);
  const Operator* BitwiseXor(LanguageMode language_mode);
  const Operator* BitwiseAnd(LanguageMode language_mode);
  const Operator* ShiftLeft(LanguageMode language_mode);
  const Operator* ShiftRight(LanguageMode language_mode);
  const Operator* ShiftRightLogical(LanguageMode language_mode);
  const Operator* Add(LanguageMode language_mode);
  const Operator* Subtract(LanguageMode language_mode);
  const Operator* Multiply(LanguageMode language_mode);
  const Operator* Divide(LanguageMode language_mode);
  const Operator* Modulus(LanguageMode language_mode);

  const Operator* UnaryNot();
  const Operator* ToBoolean();
  const Operator* ToNumber();
  const Operator* ToString();
  const Operator* ToName();
  const Operator* ToObject();
  const Operator* Yield();

  const Operator* Create();
  const Operator* CreateArguments(CreateArgumentsParameters::Type type,
                                  int start_index);
  const Operator* CreateClosure(Handle<SharedFunctionInfo> shared_info,
                                PretenureFlag pretenure);
  const Operator* CreateLiteralArray(int literal_flags);
  const Operator* CreateLiteralObject(int literal_flags);

  const Operator* CallFunction(
      size_t arity, CallFunctionFlags flags, LanguageMode language_mode,
      VectorSlotPair const& feedback = VectorSlotPair(),
      ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = NO_TAIL_CALLS);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);
  const Operator* CallConstruct(int arguments);

  const Operator* ConvertReceiver(ConvertReceiverMode convert_mode);

  const Operator* LoadProperty(LanguageMode language_mode,
                               VectorSlotPair const& feedback);
  const Operator* LoadNamed(LanguageMode language_mode, Handle<Name> name,
                            VectorSlotPair const& feedback);

  const Operator* StoreProperty(LanguageMode language_mode,
                                VectorSlotPair const& feedback);
  const Operator* StoreNamed(LanguageMode language_mode, Handle<Name> name,
                             VectorSlotPair const& feedback);

  const Operator* DeleteProperty(LanguageMode language_mode);

  const Operator* HasProperty();

  const Operator* LoadGlobal(const Handle<Name>& name,
                             const VectorSlotPair& feedback,
                             TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  const Operator* StoreGlobal(LanguageMode language_mode,
                              const Handle<Name>& name,
                              const VectorSlotPair& feedback);

  const Operator* LoadContext(size_t depth, size_t index, bool immutable);
  const Operator* StoreContext(size_t depth, size_t index);

  const Operator* LoadDynamicGlobal(const Handle<String>& name,
                                    uint32_t check_bitset,
                                    const VectorSlotPair& feedback,
                                    TypeofMode typeof_mode);
  const Operator* LoadDynamicContext(const Handle<String>& name,
                                     uint32_t check_bitset, size_t depth,
                                     size_t index);

  const Operator* TypeOf();
  const Operator* InstanceOf();

  const Operator* ForInDone();
  const Operator* ForInNext();
  const Operator* ForInPrepare();
  const Operator* ForInStep();

  const Operator* StackCheck();

  const Operator* CreateFunctionContext(int slot_count);
  const Operator* CreateCatchContext(const Handle<String>& name);
  const Operator* CreateWithContext();
  const Operator* CreateBlockContext(const Handle<ScopeInfo>& scpope_info);
  const Operator* CreateModuleContext();
  const Operator* CreateScriptContext(const Handle<ScopeInfo>& scpope_info);

 private:
  Zone* zone() const { return zone_; }

  const JSOperatorGlobalCache& cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(JSOperatorBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_OPERATOR_H_
