/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2019 (c) Matthias Konnerth
 */

#include "nodesetLoader.h"
#include "nodeset.h"
#include "util.h"
#include <libxml/SAX.h>
#include <stdio.h>
#include <string.h>
#include <charAllocator.h>

typedef enum {
    PARSER_STATE_INIT,
    PARSER_STATE_NODE,
    PARSER_STATE_DISPLAYNAME,
    PARSER_STATE_REFERENCES,
    PARSER_STATE_REFERENCE,
    PARSER_STATE_DESCRIPTION,
    PARSER_STATE_ALIAS,
    PARSER_STATE_UNKNOWN,
    PARSER_STATE_NAMESPACEURIS,
    PARSER_STATE_URI,
    PARSER_STATE_VALUE
} TParserState;

struct TParserCtx {
    TParserState state;
    TParserState prev_state;
    size_t unknown_depth;
    TNodeClass nodeClass;
    TNode *node;
    Alias* alias;
    char *onCharacters;
    void *userContext;
    struct Value *val;
    ValueInterface* valIf;
    Reference* ref;
    Nodeset* nodeset;
};

static void enterUnknownState(TParserCtx *ctx) {
    ctx->prev_state = ctx->state;
    ctx->state = PARSER_STATE_UNKNOWN;
    ctx->unknown_depth = 1;
}

static void OnStartElementNs(void *ctx, const char *localname, const char *prefix,
                             const char *URI, int nb_namespaces, const char **namespaces,
                             int nb_attributes, int nb_defaulted,
                             const char **attributes) {
    TParserCtx *pctx = (TParserCtx *)ctx;
    switch(pctx->state) {
        case PARSER_STATE_INIT:
            if(strEqual(localname, VARIABLE)) {
                pctx->nodeClass = NODECLASS_VARIABLE;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, OBJECT)) {
                pctx->nodeClass = NODECLASS_OBJECT;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, OBJECTTYPE)) {
                pctx->nodeClass = NODECLASS_OBJECTTYPE;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, DATATYPE)) {
                pctx->nodeClass = NODECLASS_DATATYPE;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, METHOD)) {
                pctx->nodeClass = NODECLASS_METHOD;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, REFERENCETYPE)) {
                pctx->nodeClass = NODECLASS_REFERENCETYPE;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, VARIABLETYPE)) {
                pctx->nodeClass = NODECLASS_VARIABLETYPE;
                pctx->node = Nodeset_newNode(pctx->nodeset, pctx->nodeClass, nb_attributes, attributes);
                pctx->state = PARSER_STATE_NODE;
            } else if(strEqual(localname, NAMESPACEURIS)) {
                pctx->state = PARSER_STATE_NAMESPACEURIS;
            } else if(strEqual(localname, ALIAS)) {
                pctx->state = PARSER_STATE_ALIAS;
                pctx->node = NULL;
                Alias *alias = Nodeset_newAlias(pctx->nodeset, nb_attributes, attributes);
                pctx->alias = alias;
                pctx->state = PARSER_STATE_ALIAS;
            } else if(strEqual(localname, "UANodeSet") ||
                      strEqual(localname, "Aliases") ||
                      strEqual(localname, "Extensions")) {
                pctx->state = PARSER_STATE_INIT;
            } else {
                enterUnknownState(pctx);
            }
            break;
        case PARSER_STATE_NAMESPACEURIS:
            if(strEqual(localname, NAMESPACEURI)) {
                Nodeset_newNamespace(pctx->nodeset);
                //pctx->nextOnCharacters = &ns->name;
                pctx->state = PARSER_STATE_URI;
            } else {
                enterUnknownState(pctx);
            }
            break;
        case PARSER_STATE_URI:
            enterUnknownState(pctx);
            break;
        case PARSER_STATE_NODE:
            if(strEqual(localname, DISPLAYNAME)) {
                //pctx->nextOnCharacters = &pctx->node->displayName;
                pctx->state = PARSER_STATE_DISPLAYNAME;
            } else if(strEqual(localname, REFERENCES)) {
                pctx->state = PARSER_STATE_REFERENCES;
            } else if(strEqual(localname, DESCRIPTION)) {
                pctx->state = PARSER_STATE_DESCRIPTION;
            } else if(strEqual(localname, VALUE)) {
                //pctx->val = pctx->valIf->(pctx->node->id);
                pctx->val = pctx->valIf->newValue(pctx->node);
                pctx->state = PARSER_STATE_VALUE;
            } else {
                enterUnknownState(pctx);
            }
            break;

        case PARSER_STATE_VALUE:
            //Value_start(pctx->val, localname);
            pctx->valIf->start(pctx->val, localname);
            break;

        case PARSER_STATE_REFERENCES:
            if(strEqual(localname, REFERENCE)) {
                pctx->state = PARSER_STATE_REFERENCE;
                //extractReferenceAttributes(pctx, nb_attributes, attributes);
                pctx->ref = Nodeset_newReference(pctx->nodeset, pctx->node, nb_attributes, attributes);
                //Nodeset_newReference
            } else {
                enterUnknownState(pctx);
            }
            break;
        case PARSER_STATE_DESCRIPTION:
            enterUnknownState(pctx);
            break;
        case PARSER_STATE_ALIAS:
            enterUnknownState(pctx);
            break;
        case PARSER_STATE_DISPLAYNAME:
            enterUnknownState(pctx);
            break;
        case PARSER_STATE_REFERENCE:
            enterUnknownState(pctx);
            break;
        case PARSER_STATE_UNKNOWN:
            pctx->unknown_depth++;
            break;
    }
    pctx->onCharacters = NULL;
}

static void OnEndElementNs(void *ctx, const char *localname, const char *prefix,
                           const char *URI) {
    TParserCtx *pctx = (TParserCtx *)ctx;
    switch(pctx->state) {
        case PARSER_STATE_INIT:
            break;
        case PARSER_STATE_ALIAS:
            Nodeset_newAliasFinish(pctx->nodeset, pctx->alias, pctx->onCharacters);
            pctx->state = PARSER_STATE_INIT;
            break;
        case PARSER_STATE_URI: {
            Nodeset_newNamespaceFinish(pctx->nodeset, pctx->userContext, pctx->onCharacters);
            pctx->state = PARSER_STATE_NAMESPACEURIS;
        } break;
        case PARSER_STATE_NAMESPACEURIS:
            pctx->state = PARSER_STATE_INIT;
            break;
        case PARSER_STATE_NODE:
            Nodeset_newNodeFinish(pctx->nodeset, pctx->node);
            pctx->state = PARSER_STATE_INIT;
            break;
        case PARSER_STATE_DISPLAYNAME:
            pctx->node->displayName=pctx->onCharacters;
            pctx->state = PARSER_STATE_NODE;
            break;
        case PARSER_STATE_REFERENCES:
            pctx->state = PARSER_STATE_NODE;
            break;
        case PARSER_STATE_REFERENCE: {
            Nodeset_newReferenceFinish(pctx->nodeset, pctx->ref, pctx->node, pctx->onCharacters);
            pctx->state = PARSER_STATE_REFERENCES;
        } break;
        case PARSER_STATE_VALUE:
            if(!strcmp(localname, VALUE)) {
                pctx->valIf->finish(pctx->val);
                ((TVariableNode*)pctx->node)->value = pctx->val;
                //Value_finish(pctx->val);
                pctx->state = PARSER_STATE_NODE;
            } else {
                pctx->valIf->end(pctx->val, localname, pctx->onCharacters);
                //Value_end(pctx->val, pctx->node, localname, pctx->onCharacters);
            }
            break;
        case PARSER_STATE_DESCRIPTION:
              pctx->state = PARSER_STATE_NODE;
              break;
        case PARSER_STATE_UNKNOWN:
            pctx->unknown_depth--;
            if(pctx->unknown_depth == 0) {
                pctx->state = pctx->prev_state;
            }
    }
    pctx->onCharacters = NULL;
}

static void OnCharacters(void *ctx, const char *ch, int len) {
    TParserCtx *pctx = (TParserCtx *)ctx;
    char *oldString = pctx->onCharacters;
    size_t oldLength = 0;
    if(oldString != NULL) {
        oldLength = strlen(oldString);
    }
    char *newValue = CharArenaAllocator_malloc(pctx->nodeset->charArena, oldLength + (size_t)len + 1);
    if(oldString != NULL) {
        memcpy(newValue, oldString, oldLength);
    }
    memcpy(newValue + oldLength, ch, (size_t)len);
    pctx->onCharacters = newValue;
}

static xmlSAXHandler make_sax_handler(void) {
    xmlSAXHandler SAXHandler;
    memset(&SAXHandler, 0, sizeof(xmlSAXHandler));
    SAXHandler.initialized = XML_SAX2_MAGIC;
    // nodesets are encoded with UTF-8
    // this code does no transformation on the encoded text or interprets it
    // so it should be safe to cast xmlChar* to char*
    SAXHandler.startElementNs = (startElementNsSAX2Func)OnStartElementNs;
    SAXHandler.endElementNs = (endElementNsSAX2Func)OnEndElementNs;
    SAXHandler.characters = (charactersSAXFunc)OnCharacters;
    return SAXHandler;
}

static int read_xmlfile(FILE *f, TParserCtx *parserCtxt) {
    char chars[1024];
    int res = (int)fread(chars, 1, 4, f);
    if(res <= 0) {
        return 1;
    }

    xmlSAXHandler SAXHander = make_sax_handler();
    xmlParserCtxtPtr ctxt =
        xmlCreatePushParserCtxt(&SAXHander, parserCtxt, chars, res, NULL);
    while((res = (int)fread(chars, 1, sizeof(chars), f)) > 0) {
        if(xmlParseChunk(ctxt, chars, res, 0)) {
            xmlParserError(ctxt, "xmlParseChunk");
            return 1;
        }
    }
    xmlParseChunk(ctxt, chars, 0, 1);
    xmlFreeParserCtxt(ctxt);
    xmlCleanupParser();
    return 0;
}

bool loadFile(const FileHandler *fileHandler) {

    if(fileHandler == NULL) {
        printf("no filehandler - return\n");
        return false;
    }
    if(fileHandler->addNamespace == NULL) {
        printf("no fileHandler->addNamespace - return\n");
        return false;
    }
    if(fileHandler->callback == NULL) {
        printf("no fileHandler->callback - return\n");
        return false;
    }
    bool status = true;
    struct timespec start, startSort, startAdd, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    Nodeset* nodeset = Nodeset_new(fileHandler->addNamespace);

    TParserCtx *ctx = (TParserCtx *)malloc(sizeof(TParserCtx));
    ctx->nodeset = nodeset;
    ctx->state = PARSER_STATE_INIT;
    ctx->prev_state = PARSER_STATE_INIT;
    ctx->unknown_depth = 0;
    ctx->onCharacters = NULL;
    ctx->userContext = fileHandler->userContext;
    ctx->valIf = fileHandler->valueHandling;

    FILE *f = fopen(fileHandler->file, "r");
    if(!f) {
        puts("file open error.");
        status = false;
        goto cleanup;
    }

    if(read_xmlfile(f, ctx)) {
        puts("xml read error.");
        status = false;
    }

    clock_gettime(CLOCK_MONOTONIC, &startSort);
    // sorting time missing
    clock_gettime(CLOCK_MONOTONIC, &startAdd);

    if(!Nodeset_getSortedNodes(nodeset, fileHandler->userContext, fileHandler->callback, ctx->valIf)) {
        status = false;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    struct timespec parse = diff(start, startSort);
    struct timespec sort = diff(startSort, startAdd);
    struct timespec add = diff(startAdd, end);
    struct timespec sum = diff(start, end);
    printf("parse (s, ms): %lu %lu\n", parse.tv_sec, parse.tv_nsec / 1000000);
    printf("sort (s, ms): %lu %lu\n", sort.tv_sec, sort.tv_nsec / 1000000);
    printf("add (s, ms): %lu %lu\n", add.tv_sec, add.tv_nsec / 1000000);
    printf("sum (s, ms): %lu %lu\n", sum.tv_sec, sum.tv_nsec / 1000000);

cleanup:
    Nodeset_cleanup(nodeset);
    free(ctx);
    if(f) {
        fclose(f);
    }
    return status;
}