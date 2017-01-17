#include "EvaluatorState.hpp"
#include "EvalObject.hpp"
#include "Evaluator.hpp"
#include "Util.hpp"
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
using namespace std;
namespace RapidHLS {

VariableDir::VariableDir(bool _input, bool _output, bool _toplvl)
    : is_input(_input), is_output(_output), is_toplevel(_toplvl){};

/* EvaluatorVariable base*/

EvaluatorVariable::EvaluatorVariable(VariableDir _dir) : dir(_dir) {}
EvaluatorVariable::EvaluatorVariable(VariableDir _dir, string _name)
    : name(_name), dir(_dir){};
EvaluatorVariable::EvaluatorVariable(VariableDir _dir, string _name,
                                     const AttributeSet &_attr)
    : name(_name), attributes(_attr), dir(_dir){};

EvaluatorVariable *EvaluatorVariable::Create(VariableDir _dir, string _name,
                                             DataType *_type, bool _is_static) {
  // TODO: restructure this once in the long future; perhaps once a better
  // typing system is created
  IntegerType *it = dynamic_cast<IntegerType *>(_type);
  ArrayType *arrt = dynamic_cast<ArrayType *>(_type);
  StructureType *st = dynamic_cast<StructureType *>(_type);
  RAMType *rt = dynamic_cast<RAMType *>(_type);
  if (it != nullptr) {
    return new ScalarEvaluatorVariable(_dir, _name, it, _is_static);
  } else if (arrt != nullptr) {
    return new ArrayEvaluatorVariable(_dir, _name, arrt, _is_static);
  } else if (rt != nullptr) {
    return new ExternalMemoryEvaluatorVariable(_dir, _name, rt);
  } else if (st != nullptr) {
    return new StructureEvaluatorVariable(_dir, _name, st, _is_static);
  } else {
    throw eval_error("unable to create variable ===" + _name +
                     "===: unsupported type");
  }
}

vector<EvaluatorVariable *> EvaluatorVariable::GetArrayChildren() {
  return vector<EvaluatorVariable *>{};
}

vector<EvaluatorVariable *> EvaluatorVariable::GetAllChildren() {
  return vector<EvaluatorVariable *>{};
}

EvaluatorVariable *EvaluatorVariable::GetChildByName(string name) {
  throw eval_error("variable ===" + name + "=== does not contain member ===" +
                   name + "===");
}

bool EvaluatorVariable::HasDefaultValue() { return false; }

BitConstant EvaluatorVariable::GetDefaultValue() {
  throw eval_error("variable ===" + name + "=== does not have default value");
}

void EvaluatorVariable::HandleWrite(Evaluator *genst, EvalObject *value) {
  genst->SetVariableValue(this, value);
}

bool EvaluatorVariable::IsNonTrivialArrayAccess() { return false; };

EvalObject *
EvaluatorVariable::HandleSubscriptedRead(Evaluator *genst,
                                         vector<EvalObject *> index) {
  throw eval_error("HandleSubscriptedRead not supported for variable ===" +
                   name + "===");
};

void EvaluatorVariable::HandleSubscriptedWrite(Evaluator *genst,
                                               vector<EvalObject *> index,
                                               EvalObject *value) {
  throw eval_error("HandleSubscriptedWrite not supported for variable ===" +
                   name + "===");
}

void EvaluatorVariable::HandlePush(Evaluator *genst, EvalObject *value) {
  throw eval_error("push (operator<<) not supported for variable ===" + name +
                   "===");
}

EvalObject *EvaluatorVariable::HandlePop(Evaluator *genst) {
  throw eval_error("pop (operator>>) not supported for variable ===" + name +
                   "===");
}

void EvaluatorVariable::SetBitOffset(int _bitoffset) {
  bitoffset = _bitoffset;
};

int EvaluatorVariable::GetBitOffset() { return bitoffset; }

VariableDir EvaluatorVariable::GetDir() { return dir; }

/* ScalarEvaluatorVariable */
ScalarEvaluatorVariable::ScalarEvaluatorVariable(VariableDir _dir, string _name,
                                                 IntegerType *_type,
                                                 bool _is_static)
    : EvaluatorVariable(_dir, _name), type(_type), is_static(_is_static) {
  if (is_static) {
    dir.is_input = true;
    write_enable = new ScalarEvaluatorVariable(
        VariableDir(false, true, false), name + "___wren",
        new IntegerType(1, false), false);
    write_enable->hasDefaultValue = true;
    write_enable->defaultValue = BitConstant(0);
    written_value = new ScalarEvaluatorVariable(VariableDir(false, true, false),
                                                name + "___wrval", type, false);
    written_value->hasDefaultValue = true;
    written_value->defaultValue =
        BitConstant(0); // arguablly this should be "don't care"
  }
}

DataType *ScalarEvaluatorVariable::GetType() { return type; };
bool ScalarEvaluatorVariable::IsScalar() { return true; };
bool ScalarEvaluatorVariable::HasDefaultValue() { return hasDefaultValue; };
BitConstant ScalarEvaluatorVariable::GetDefaultValue() { return defaultValue; };
void ScalarEvaluatorVariable::SetDefaultValue(BitConstant defval) {
  hasDefaultValue = true;
  defaultValue = defval;
};

EvaluatorVariable *ScalarEvaluatorVariable::GetChildByName(string name) {
  if (is_static) {
    if (name == "__wren") {
      return write_enable;
    } else if (name == "___wrval") {
      return written_value;
    }
  }
  // Unless any more special meta-children are added, this will always throw
  return EvaluatorVariable::GetChildByName(name);
}

vector<EvaluatorVariable *> ScalarEvaluatorVariable::GetAllChildren() {
  if (is_static) {
    return vector<EvaluatorVariable *>{write_enable, written_value};
  } else {
    return vector<EvaluatorVariable *>{};
  }
}

void ScalarEvaluatorVariable::HandleWrite(Evaluator *genst, EvalObject *value) {
  if (is_static) {
    genst->SetVariableValue(written_value, value);
    genst->SetVariableValue(write_enable, new EvalConstant(BitConstant(1)));
  }
  genst->SetVariableValue(this, value);
}

ArrayEvaluatorVariable::ArrayEvaluatorVariable(VariableDir _dir, string _name,
                                               ArrayType *_type,
                                               bool _is_static)
    : EvaluatorVariable(_dir, _name), type(_type) {
  for (int i = 0; i < type->length; i++) {
    arrayItems.push_back(EvaluatorVariable::Create(
        VariableDir(dir.is_input, dir.is_output, false),
        _name + "___itm" + to_string(i), type->baseType, _is_static));
  }

  SetBitOffset(0);
}

DataType *ArrayEvaluatorVariable::GetType() { return type; }

bool ArrayEvaluatorVariable::IsScalar() { return false; };

vector<EvaluatorVariable *> ArrayEvaluatorVariable::GetArrayChildren() {
  return arrayItems;
}

vector<EvaluatorVariable *> ArrayEvaluatorVariable::GetAllChildren() {
  return arrayItems;
}

void ArrayEvaluatorVariable::SetBitOffset(int _bitoffset) {
  int offset = _bitoffset;
  for_each(arrayItems.begin(), arrayItems.end(), [&](EvaluatorVariable *c) {
    c->SetBitOffset(offset);
    offset += type->baseType->GetWidth();
  });
  EvaluatorVariable::SetBitOffset(_bitoffset);
}

StructureEvaluatorVariable::StructureEvaluatorVariable(VariableDir _dir,
                                                       string _name,
                                                       StructureType *_type,
                                                       bool _is_static)
    : EvaluatorVariable(_dir, _name), type(_type) {
  transform(type->content.begin(), type->content.end(),
            back_inserter(structItems), [&](DataStructureItem itm) {
              return EvaluatorVariable::Create(
                  VariableDir(dir.is_input, dir.is_output, dir.is_toplevel),
                  name + "_" + itm.name, itm.type, _is_static);
            });
};

DataType *StructureEvaluatorVariable::GetType() { return type; };

bool StructureEvaluatorVariable::IsScalar() { return false; };

vector<EvaluatorVariable *> StructureEvaluatorVariable::GetAllChildren() {
  return structItems;
};

EvaluatorVariable *StructureEvaluatorVariable::GetChildByName(string name) {
  // find item in struct
  auto iter =
      find_if(type->content.begin(), type->content.end(),
              [name](DataStructureItem itm) { return itm.name == name; });
  if (iter == type->content.end()) {
    // this will probably throw, as 'name' probably isn't a member
    return EvaluatorVariable::GetChildByName(name);
  } else {
    return structItems[distance(type->content.begin(), iter)];
  }
}

void StructureEvaluatorVariable::SetBitOffset(int _bitoffset) {
  int offset = _bitoffset;
  for_each(structItems.begin(), structItems.end(), [&](EvaluatorVariable *c) {
    c->SetBitOffset(offset);
    offset += c->GetType()->GetWidth();
  });
  EvaluatorVariable::SetBitOffset(_bitoffset);
}

ExternalMemoryEvaluatorVariable::ExternalMemoryEvaluatorVariable(
    VariableDir _dir, string _name, RAMType *_type)
    : EvaluatorVariable(_dir, _name), type(_type) {
  ports["__address"] = new ScalarEvaluatorVariable(
      VariableDir(false, true, _dir.is_toplevel), _name + "_address",
      new IntegerType(GetAddressBusSize(type->length), false), false);
  ports["__address"]->SetDefaultValue(BitConstant(0));

  ports["__q"] =
      new ScalarEvaluatorVariable(VariableDir(true, false, _dir.is_toplevel),
                                  _name + "_q", &(type->baseType), false);

  if (!type->is_rom) {
    ports["__wren"] = new ScalarEvaluatorVariable(
        VariableDir(false, true, _dir.is_toplevel), _name + "_wren",
        new IntegerType(1, false), false);
    ports["__wren"]->SetDefaultValue(BitConstant(0));

    ports["__data"] =
        new ScalarEvaluatorVariable(VariableDir(false, true, _dir.is_toplevel),
                                    _name + "_data", &(type->baseType), false);
  }
  dir.is_toplevel = false;
};

DataType *ExternalMemoryEvaluatorVariable::GetType() { return type; }
bool ExternalMemoryEvaluatorVariable::IsScalar() { return false; }

vector<EvaluatorVariable *> ExternalMemoryEvaluatorVariable::GetAllChildren() {
  vector<EvaluatorVariable *> children;
  transform(ports.begin(), ports.end(), back_inserter(children),
            [](pair<string, ScalarEvaluatorVariable *> c) { return c.second; });
  return children;
};

EvaluatorVariable *
ExternalMemoryEvaluatorVariable::GetChildByName(string name) {
  auto iter = ports.find(name);
  if (iter != ports.end()) {
    return iter->second;
  } else {
    return EvaluatorVariable::GetChildByName(name);
  }
}

MemoryDeviceParameters ExternalMemoryEvaluatorVariable::GetMemoryParams() {
  MemoryDeviceParameters mdp;
  mdp.canRead = true;
  mdp.canWrite = !type->is_rom;
  mdp.hasRden = false;
  mdp.hasWren = mdp.canWrite;
  mdp.readLatency = 1;
  mdp.seperateRWports = false;
  return mdp;
}

bool ExternalMemoryEvaluatorVariable::IsNonTrivialArrayAccess() {
  return true;
};

EvalObject *ExternalMemoryEvaluatorVariable::HandleSubscriptedRead(
    Evaluator *genst, vector<EvalObject *> index) {
  // perhaps should warn if already accessed?
  if (index.size() != 1) {
    throw eval_error("invalid dimensions for access to variable ===" + name +
                     "===");
  }
  genst->SetVariableValue(ports.at("__address"), index[0]);
  return new EvalVariable(ports.at("__q"));
}

void ExternalMemoryEvaluatorVariable::HandleSubscriptedWrite(
    Evaluator *genst, vector<EvalObject *> index, EvalObject *value) {
  if (index.size() != 1) {
    throw eval_error("invalid dimensions for access to variable ===" + name +
                     "===");
  }
  if (type->is_rom) {
    throw eval_error("cannot write to ROM type variable ===" + name + "===");
  }
  genst->SetVariableValue(ports.at("__address"), index[0]);
  genst->SetVariableValue(ports.at("__wren"), new EvalConstant(BitConstant(1)));
  genst->SetVariableValue(ports.at("__data"), value);
}

StreamEvaluatorVariable::StreamEvaluatorVariable(VariableDir _dir, string _name,
                                                 StreamType *_type)
    : EvaluatorVariable(_dir, _name), type(_type) {
  int totalSize;
  if (type->isStream2d) {
    totalSize = type->length * type->height;
  } else {
    totalSize = type->length;
  }
  for (int i = 0; i < totalSize; i++) {
    streamWindow.push_back(EvaluatorVariable::Create(
        VariableDir(true, false, false), _name + "___itm" + to_string(i),
        type->baseType, false));
  }
  written_value = EvaluatorVariable::Create(
      VariableDir(false, true, false), _name + "___wrval", type->baseType,
      false); // TODO: default value for this
  write_enable = new ScalarEvaluatorVariable(VariableDir(false, true, false),
                                             name + "___wren",
                                             new IntegerType(1, false), false);
  write_enable->SetDefaultValue(BitConstant(0));
  dir.is_toplevel = false;
};

DataType *StreamEvaluatorVariable::GetType() { return type; };

bool StreamEvaluatorVariable::IsScalar() { return false; };

vector<EvaluatorVariable *> StreamEvaluatorVariable::GetAllChildren() {
  vector<EvaluatorVariable *> children;
  children.insert(children.end(), streamWindow.begin(), streamWindow.end());
  children.push_back(write_enable);
  children.push_back(written_value);
  return children;
}
vector<EvaluatorVariable *> StreamEvaluatorVariable::GetArrayChildren() {
  return streamWindow;
}
void StreamEvaluatorVariable::HandlePush(Evaluator *genst, EvalObject *value) {
  genst->SetVariableValue(write_enable, new EvalConstant(BitConstant(1)));
  genst->SetVariableValue(written_value, value);
}
}
