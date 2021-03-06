#include "Ob.h"
#include "Code.h"
#include "Prim.h"
#include "Number.h"
#include "Expr.h"
#include "RblAtom.h"

#include "CommandLine.h"

#include <google/protobuf/text_format.h>
#include <Ob.pb.h>

#include <fstream>
#include <string>
#include <map>

// The following functions and table orchestrate the handling of
// Rosette Object export within the litvec portion of the exported object code.
// To add support for an additional object type:
//  - define a protobuf object for it in Ob.proto
//  - write a handler that populates the protobuf from the Rosette Ob
//  - add it to the handlers table.
//

typedef std::string ExportObjectKey;
typedef void (*ExportObjectHandler)(ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype);
typedef ObjectCodePB::ObType ExportObjectType;
extern std::map<ExportObjectKey, std::pair<ExportObjectHandler, ExportObjectType> >  handlers;

// Handlers to populate specific objects
void defaultObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
}

// This function looks up an object in the handlers table based on its type string
// and then calls the appropriate handler to populate it.
std::set<IdType> exportedIds;
void populateObjectByType(pOb ob, ObjectCodePB::Object *exOb) {

    exOb->set_id(BASE(ob)->id);

    auto id = exportedIds.find(BASE(ob)->id);
    if (id != exportedIds.end()) {
        // Don't export an object more than once. This effectively eliminates problems
        // with self referential objects and loops in the object tree.

        // Already exported. just reference the id.
        exOb->set_reference(true);
    } else {
        std::string type = BASE(ob)->typestring();
        auto oh = handlers.find(type);
        if (oh != handlers.end()) {
            ExportObjectHandler handler = oh->second.first;
            ExportObjectType obType = oh->second.second;

            exOb->set_type(obType);
            exOb->set_metaid(BASE(ob)->meta()->id);
            exOb->set_parentid(BASE(ob)->parent()->id);

            exportedIds.insert(BASE(ob)->id);
            handler(exOb, ob, obType);
        } else {
            warning("Exporting object type %s not yet implemented!", type.c_str());
            exOb->set_type(ObjectCodePB::OT_unknown);
            defaultObjectHandler(exOb, ob, ObjectCodePB::OT_unknown);
        }
    }
}

void fixnumObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Fixnum *fn = lvob->mutable_fixnum();
    fn->set_value(FIXVAL(ob));
}

void charObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Char *ch = lvob->mutable_char_();
    ch->set_value(CHARVAL(ob));
}

void codeObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
}

void floatObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Float *fl = lvob->mutable_float_();
    fl->set_value(((Float*)(ob))->val);
}

void symbolObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Symbol *sym = lvob->mutable_symbol();
    sym->set_name(BASE(ob)->asPathname());
}

void rblboolObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::RblBool *bl = lvob->mutable_rblbool();
    bl->set_value(RBLBOOL(ob));
}
void rblstringObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::RBLstring *str = lvob->mutable_rblstring();
    str->set_value(BASE(ob)->asPathname());
}

void tupleObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype) {
    ObjectCodePB::Tuple *pbTup = lvob->mutable_tuple();
    Tuple * tup = (Tuple *)ob;

    // A tuple is a list of objects.  Retrieve and call the handler for each of them.
    for (int i = 0; i < tup->numberOfElements(); i++) {
        pOb ob = tup->elem(i);
        std::string type = BASE(tup->elem(i))->typestring();
        ObjectCodePB::Object *elOb = pbTup->add_elements();
        populateObjectByType(ob, elOb);
    }
}

void blockExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::BlockExpr *blob = lvob->mutable_blockexpr();
    BlockExpr * be = (BlockExpr *)ob;

    ObjectCodePB::Object *subexprs = blob->mutable_subexprs();
    populateObjectByType(be->subExprs, subexprs);

    ObjectCodePB::Object *implicit = blob->mutable_implicit();
    populateObjectByType(be->implicit, implicit);
}

void actorObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Actor *actob = lvob->mutable_actor();
    Actor * act = (Actor *)ob;

    ObjectCodePB::Object *extension = actob->mutable_extension();
    populateObjectByType(act->extension, extension);
}

void nivObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::Niv *nivob = lvob->mutable_niv();
}

void stdExtensionObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::StdExtension *sextob = lvob->mutable_stdextension();
}

void expandedLocationObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::ExpandedLocation *exlocob = lvob->mutable_expandedlocation();
}

void freeExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::FreeExpr *feob = lvob->mutable_freeexpr();
    FreeExpr * fe = (FreeExpr *)ob;

    ObjectCodePB::Object *freeids = feob->mutable_freeids();
    populateObjectByType(fe->freeIds, freeids);

    ObjectCodePB::Object *body = feob->mutable_body();
    populateObjectByType(fe->body, body);
}

void gotoExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::GotoExpr *gtob = lvob->mutable_gotoexpr();
    GotoExpr * ge = (GotoExpr *)ob;

    ObjectCodePB::Object *label = gtob->mutable_label();
    populateObjectByType(ge->label, label);
}

void ifExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::IfExpr *ifob = lvob->mutable_ifexpr();
    IfExpr * ie = (IfExpr *)ob;

    ObjectCodePB::Object *condition = ifob->mutable_condition();
    populateObjectByType(ie->condition, condition);

    ObjectCodePB::Object *trueBranch = ifob->mutable_truebranch();
    populateObjectByType(ie->trueBranch, trueBranch);

    ObjectCodePB::Object *falseBranch = ifob->mutable_falsebranch();
    populateObjectByType(ie->falseBranch, falseBranch);
}

void labelExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::LabelExpr *leob = lvob->mutable_labelexpr();
    LabelExpr * le = (LabelExpr *)ob;

    ObjectCodePB::Object *label = leob->mutable_label();
    populateObjectByType(le->label, label);

    ObjectCodePB::Object *body = leob->mutable_body();
    populateObjectByType(le->body, body);
}

void letExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::LetExpr *leob = lvob->mutable_letexpr();
    LetExpr * le = (LetExpr *)ob;

    ObjectCodePB::Object *bindings = leob->mutable_bindings();
    populateObjectByType(le->bindings, bindings);

    ObjectCodePB::Object *body = leob->mutable_body();
    populateObjectByType(le->body, body);
}

void tupleExprObjectHandler( ObjectCodePB::Object * lvob, pOb ob, ObjectCodePB::ObType pbtype ) {
    ObjectCodePB::TupleExpr *teob = lvob->mutable_tupleexpr();
    TupleExpr * te = (TupleExpr *)ob;

    ObjectCodePB::Object *rest = teob->mutable_rest();
    populateObjectByType(te->rest, rest);
}



// The handler table that defines which objects are supported, and how they are handled.

std::map<ExportObjectKey, std::pair<ExportObjectHandler, ExportObjectType> >  handlers = {
    {"Actor",               {actorObjectHandler,     ObjectCodePB::OT_Actor} },
    {"BlockExpr",           {blockExprObjectHandler, ObjectCodePB::OT_BlockExpr} },
    {"Char",                {charObjectHandler,      ObjectCodePB::OT_Char} },
    {"Code",                {codeObjectHandler,      ObjectCodePB::OT_Code} },
    {"ExpandedLocation",    {expandedLocationObjectHandler, ObjectCodePB::OT_ExpandedLocation} },
    {"Fixnum",              {fixnumObjectHandler,    ObjectCodePB::OT_Fixnum} },
    {"Float",               {floatObjectHandler,     ObjectCodePB::OT_Float} },
    {"FreeExpr",            {freeExprObjectHandler,  ObjectCodePB::OT_FreeExpr} },
    {"GotoExpr",            {gotoExprObjectHandler,  ObjectCodePB::OT_GotoExpr} },
    {"IfExpr",              {ifExprObjectHandler,    ObjectCodePB::OT_IfExpr} },
    {"LabelExpr",           {labelExprObjectHandler, ObjectCodePB::OT_LabelExpr} },
    {"LetExpr",             {letExprObjectHandler,   ObjectCodePB::OT_LetExpr} },
    {"LetrecExpr",          {defaultObjectHandler,   ObjectCodePB::OT_LetrecExpr} },
    {"MethodExpr",          {defaultObjectHandler,   ObjectCodePB::OT_MethodExpr} },
    {"Niv",                 {nivObjectHandler,       ObjectCodePB::OT_Niv}},
    {"NullExpr",            {defaultObjectHandler,   ObjectCodePB::OT_NullExpr}},
    {"Proc",                {defaultObjectHandler,   ObjectCodePB::OT_Proc} },
    {"ProcExpr",            {defaultObjectHandler,   ObjectCodePB::OT_ProcExpr} },
    {"QuoteExpr",           {defaultObjectHandler,   ObjectCodePB::OT_QuoteExpr} },
    {"RblAtom",             {rblboolObjectHandler,   ObjectCodePB::OT_RblAtom}},
    {"RblBool",             {rblboolObjectHandler,   ObjectCodePB::OT_RblBool}},
    {"RBLstring",           {rblstringObjectHandler, ObjectCodePB::OT_RBLstring} },
    {"ReflectiveMethodExpr",{defaultObjectHandler,   ObjectCodePB::OT_ReflectiveMethodExpr} },
    {"RequestExpr",         {defaultObjectHandler,   ObjectCodePB::OT_RequestExpr} },
    {"SendExpr",            {defaultObjectHandler,   ObjectCodePB::OT_SendExpr} },
    {"SeqExpr",             {defaultObjectHandler,   ObjectCodePB::OT_SeqExpr} },
    {"SetExpr",             {defaultObjectHandler,   ObjectCodePB::OT_SetExpr} },
    {"StdExtension",        {stdExtensionObjectHandler, ObjectCodePB::OT_StdExtension} },
    {"StdMthd",             {defaultObjectHandler,   ObjectCodePB::OT_StdMthd} },
    {"Symbol",              {symbolObjectHandler,    ObjectCodePB::OT_Symbol} },
    {"TblObject",           {defaultObjectHandler,   ObjectCodePB::OT_TblObject} },
    {"Template",            {defaultObjectHandler,   ObjectCodePB::OT_Template} },
    {"Tuple",               {tupleObjectHandler,     ObjectCodePB::OT_Tuple} },
    {"TupleExpr",           {tupleExprObjectHandler, ObjectCodePB::OT_TupleExpr} },
};

IdType createObjectId(Code * code) {
    // Rosette internally, uses the object pointer but for export, this results in
    // duplicate ids due to objects being garbage collected, and then the memory
    // being reused.

    // Here we will use the newly invented object ID that is generated when the
    // object is created by Rosette

    return BASE(code)->id;
}



// This is the protobuf that contains exported object code.
ObjectCodePB::ObjectCode objectCode;


// The collection routine that processes individual Code objects as they are compiled.
void collectExportCode(Code *code) {
    if (VerboseFlag) fprintf(stderr, "\n%s\n", __PRETTY_FUNCTION__);

    IdType codeId = createObjectId( code );

    if (VerboseFlag) {
        fprintf(stderr, "CodeId=%llu\n", codeId);
        code->dumpOn(stderr);
    }

    if ('\0' == *ExportFile)
        return;

    // Build the code block protobuf to export
    ObjectCodePB::CodeBlock * cb = objectCode.add_codeblock();

    // Save the code id
    cb->set_id( codeId );

    // Get access to the codevec and litvec to be exported.
    CodeVec * codevec = code->codevec;
    Tuple * litvec = code->litvec;

    // Save the binary opCodes from the codevec
    const char * code_binary = (char *)codevec->absolutize(0);
    size_t code_size = codevec->numberOfWords() * sizeof(uint32_t);
    ObjectCodePB::CodeVec * cv = cb->mutable_codevec();
    cv->set_opcodes( std::string(code_binary, code_size) );

    // Create the export litvec
    ObjectCodePB::LitVec * lv = cb->mutable_litvec();

    // Add objects or references to them to the export litvec
    for (int i = 0; i < litvec->numberOfElements(); i++) {
        pOb ob = litvec->elem(i);
        ObjectCodePB::Object *lvob = lv->add_ob();
        populateObjectByType(ob, lvob);
    }
}

// Write the collected object code to disk
void writeExportCode() {
    if ('\0' == *ExportFile)
        return;

    // Text output of the protobuf for debugging
    if (VerboseFlag) {
        std::string s;
        google::protobuf::TextFormat::PrintToString( objectCode, &s );
        fprintf(stderr, "ObjectCode = \n%s\n", s.c_str());
    }

    // Write the object code to disk.
    std::fstream output(ExportFile, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!objectCode.SerializeToOstream(&output)) {
      warning("Failed to write object code to %s.", ExportFile);
    }
}
