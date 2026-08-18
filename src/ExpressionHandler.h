// ExpressionHandler.h
// Expression pedal handler - supports two types:
//
// EXPR_ANALOG:   Reads an analog pin, scales raw ADC value using
//                exprAnalogMin/exprAnalogMax to produce CC value 0-127.
//                Serial debug prints raw value whenever it changes.
// EXPR_DISCRETE: Counts HIGH inputs in a CWB range, CC value = count of HIGHs.
//                When value changes, sends all intermediate CC values in one burst.

#ifndef EXPRESSION_HANDLER_H
#define EXPRESSION_HANDLER_H

#include "OrganCore.h"
#include "ScanChain.h"

void expressionInit();
void processExpressions();

#endif // EXPRESSION_HANDLER_H
