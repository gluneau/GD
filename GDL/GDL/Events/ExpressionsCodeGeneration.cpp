/** \file
 *  Game Develop
 *  2008-2012 Florian Rival (Florian.Rival@gmail.com)
 */
#if defined(GD_IDE_ONLY)

#include "ExpressionsCodeGeneration.h"
#include "GDL/StrExpressionInstruction.h"
#include "GDL/ExpressionInstruction.h"
#include "GDL/IDE/GDExpressionParser.h"
#include "GDL/Events/EventsCodeGenerator.h"
#include "GDL/ExtensionBase.h"
#include "GDL/Scene.h"
#include "GDL/ExtensionsManager.h"
#include "GDL/Events/EventsCodeNameMangler.h"
#include "GDL/Events/EventsCodeGenerationContext.h"
#include "GDL/CommonTools.h"
#include "GDL/Game.h"
#include "GDL/Scene.h"

using namespace std;

CallbacksForGeneratingExpressionCode::CallbacksForGeneratingExpressionCode(string & plainExpression_, const Game & game_, const Scene & scene_, EventsCodeGenerator & codeGenerator_, EventsCodeGenerationContext & context_) :
    plainExpression(plainExpression_),
    game(game_),
    scene(scene_),
    codeGenerator(codeGenerator_),
    context(context_)
{

}

void CallbacksForGeneratingExpressionCode::OnConstantToken(string text)
{
    plainExpression += text;
};
void CallbacksForGeneratingExpressionCode::OnOperator(string text)
{
    plainExpression += text;
};
void CallbacksForGeneratingExpressionCode::OnNumber(string text)
{
    if (text.find_first_of(".e") == std::string::npos)
        text += ".0";

    plainExpression += text;
};

void CallbacksForGeneratingExpressionCode::OnStaticFunction(string functionName, const std::vector<GDExpression> & parameters, const ExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<ExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }


    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);

    string parametersStr;
    for (unsigned int i = 0;i<parametersCode.size();++i)
    {
        if ( i != 0 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    plainExpression += expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+")";
};

void CallbacksForGeneratingExpressionCode::OnStaticFunction(string functionName, const std::vector<GDExpression> & parameters, const StrExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<StrExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }

    //TODO : A bit of hack here..
    //Special case : Function without name is a litteral string.
    if ( functionName.empty() )
    {
        if ( parameters.empty() ) return;
        plainExpression += "std::string(\""+codeGenerator.ConvertToCppString(parameters[0].GetPlainString())+"\")";

        return;
    }

    //Prepare parameters
    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);
    string parametersStr;
    for (unsigned int i = 0;i<parametersCode.size();++i)
    {
        if ( i != 0 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    plainExpression += expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+")";
};

void CallbacksForGeneratingExpressionCode::OnObjectFunction(string functionName, const std::vector<GDExpression> & parameters, const ExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);
    if ( parameters.empty() ) return;

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<ExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }

    //Prepare parameters
    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);
    string parametersStr;
    for (unsigned int i = 1;i<parametersCode.size();++i)
    {
        if ( i != 1 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    //Get object(s) concerned by function call
    vector< ObjectGroup >::const_iterator globalGroup = find_if(game.objectGroups.begin(), game.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));
    vector< ObjectGroup >::const_iterator sceneGroup = find_if(scene.objectGroups.begin(), scene.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));

    std::vector<std::string> realObjects; //With groups, we may have to generate expression for more than one object list.
    if ( globalGroup != game.objectGroups.end() )
        realObjects = (*globalGroup).GetAllObjectsNames();
    else if ( sceneGroup != scene.objectGroups.end() )
        realObjects = (*sceneGroup).GetAllObjectsNames();
    else
        realObjects.push_back(parameters[0].GetPlainString());

    //If current object is present, use it and only it.
    if ( find(realObjects.begin(), realObjects.end(), context.GetCurrentObject()) != realObjects.end() )
    {
        realObjects.clear();
        realObjects.push_back(context.GetCurrentObject());
    }

    std::string output = "0";
    for (unsigned int i = 0;i<realObjects.size();++i)
    {
        context.ObjectsListNeeded(realObjects[i]);

        //Cast the object if needed
        string objectType = GetTypeOfObject(game, scene, realObjects[i]);
        const ExtensionObjectInfos & objInfo = GDpriv::ExtensionsManager::GetInstance()->GetObjectInfo(objectType);
        bool castNeeded = !objInfo.cppClassName.empty();

        //Build string to access the object
        codeGenerator.AddIncludeFile(objInfo.optionalIncludeFile);
        if ( context.GetCurrentObject() == realObjects[i]  && !context.GetCurrentObject().empty())
        {
            if ( !castNeeded )
                output = "("+ManObjListName(realObjects[i])+"[i]->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(static_cast<"+objInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[i])->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
        else
        {
            if ( !castNeeded )
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" :"+ ManObjListName(realObjects[i])+"[0]->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" : "+"static_cast<"+objInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[0])->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
    }

    plainExpression += output;
};

void CallbacksForGeneratingExpressionCode::OnObjectFunction(string functionName, const std::vector<GDExpression> & parameters, const StrExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);
    if ( parameters.empty() ) return;

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<StrExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }

    //Prepare parameters
    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);
    string parametersStr;
    for (unsigned int i = 1;i<parametersCode.size();++i)
    {
        if ( i != 1 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    //Get object(s) concerned by function call
    vector< ObjectGroup >::const_iterator globalGroup = find_if(game.objectGroups.begin(), game.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));
    vector< ObjectGroup >::const_iterator sceneGroup = find_if(scene.objectGroups.begin(), scene.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));

    std::vector<std::string> realObjects; //With groups, we may have to generate expression for more than one object list.
    if ( globalGroup != game.objectGroups.end() )
        realObjects = (*globalGroup).GetAllObjectsNames();
    else if ( sceneGroup != scene.objectGroups.end() )
        realObjects = (*sceneGroup).GetAllObjectsNames();
    else
        realObjects.push_back(parameters[0].GetPlainString());

    //If current object is present, use it and only it.
    if ( find(realObjects.begin(), realObjects.end(), context.GetCurrentObject()) != realObjects.end() )
    {
        realObjects.clear();
        realObjects.push_back(context.GetCurrentObject());
    }

    std::string output = "\"\"";
    for (unsigned int i = 0;i<realObjects.size();++i)
    {
        context.ObjectsListNeeded(realObjects[i]);

        //Cast the object if needed
        string objectType = GetTypeOfObject(game, scene, realObjects[i]);
        const ExtensionObjectInfos & objInfo = GDpriv::ExtensionsManager::GetInstance()->GetObjectInfo(objectType);
        bool castNeeded = !objInfo.cppClassName.empty();

        //Build string to access the object
        codeGenerator.AddIncludeFile(objInfo.optionalIncludeFile);
        if ( context.GetCurrentObject() == realObjects[i]  && !context.GetCurrentObject().empty())
        {
            if ( !castNeeded )
                output = "("+ManObjListName(realObjects[i])+"[i]->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(static_cast<"+objInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[i])->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
        else
        {
            if ( !castNeeded )
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" :"+ ManObjListName(realObjects[i])+"[0]->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" : "+"static_cast<"+objInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[0])->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
    }

    plainExpression += output;
};

void CallbacksForGeneratingExpressionCode::OnObjectAutomatismFunction(string functionName, const std::vector<GDExpression> & parameters, const ExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);
    if ( parameters.size() < 2 ) return;

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<ExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }

    //Prepare parameters
    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);
    string parametersStr;
    for (unsigned int i = 2;i<parametersCode.size();++i)
    {
        if ( i != 2 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    //Get object(s) concerned by function call
    vector< ObjectGroup >::const_iterator globalGroup = find_if(game.objectGroups.begin(), game.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));
    vector< ObjectGroup >::const_iterator sceneGroup = find_if(scene.objectGroups.begin(), scene.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));

    std::vector<std::string> realObjects; //With groups, we may have to generate expression for more than one object list.
    if ( globalGroup != game.objectGroups.end() )
        realObjects = (*globalGroup).GetAllObjectsNames();
    else if ( sceneGroup != scene.objectGroups.end() )
        realObjects = (*sceneGroup).GetAllObjectsNames();
    else
        realObjects.push_back(parameters[0].GetPlainString());

    //If current object is present, use it and only it.
    if ( find(realObjects.begin(), realObjects.end(), context.GetCurrentObject()) != realObjects.end() )
    {
        realObjects.clear();
        realObjects.push_back(context.GetCurrentObject());
    }

    std::string output = "0";
    for (unsigned int i = 0;i<realObjects.size();++i)
    {
        context.ObjectsListNeeded(realObjects[i]);

        //Cast the object if needed
        string automatismType = GetTypeOfAutomatism(game, scene, parameters[1].GetPlainString());
        const AutomatismInfo & autoInfo = GDpriv::ExtensionsManager::GetInstance()->GetAutomatismInfo(automatismType);
        bool castNeeded = !autoInfo.cppClassName.empty();

        //Build string to access the automatism
        codeGenerator.AddIncludeFile(autoInfo.optionalIncludeFile);
        if ( context.GetCurrentObject() == realObjects[i]  && !context.GetCurrentObject().empty() )
        {
            if ( !castNeeded )
                output = "("+ManObjListName(realObjects[i])+"[i]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\")->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(static_cast<"+autoInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[i]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\"))->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
        else
        {
            if ( !castNeeded )
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" :"+ManObjListName(realObjects[i])+"[0]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\")->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" : "+"static_cast<"+autoInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[0]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\"))->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
    }

    plainExpression += output;
};

void CallbacksForGeneratingExpressionCode::OnObjectAutomatismFunction(string functionName, const std::vector<GDExpression> & parameters, const StrExpressionMetadata & expressionInfo)
{
    codeGenerator.AddIncludeFile(expressionInfo.cppCallingInformation.optionalIncludeFile);
    if ( parameters.size() < 2 ) return;

    //Launch custom code generator if needed
    if ( expressionInfo.cppCallingInformation.optionalCustomCodeGenerator != boost::shared_ptr<StrExpressionMetadata::CppCallingInformation::CustomCodeGenerator>() )
    { plainExpression += expressionInfo.cppCallingInformation.optionalCustomCodeGenerator->GenerateCode(game, scene, parameters, codeGenerator, context); return; }

    //Prepare parameters
    vector<string> parametersCode = codeGenerator.GenerateParametersCodes(game, scene, parameters, expressionInfo.parameters, context);
    string parametersStr;
    for (unsigned int i = 2;i<parametersCode.size();++i)
    {
        if ( i != 2 ) parametersStr += ", ";
        parametersStr += parametersCode[i];
    }

    //Get object(s) concerned by function call
    vector< ObjectGroup >::const_iterator globalGroup = find_if(game.objectGroups.begin(), game.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));
    vector< ObjectGroup >::const_iterator sceneGroup = find_if(scene.objectGroups.begin(), scene.objectGroups.end(), bind2nd(HasTheSameName(), parameters[0].GetPlainString()));

    std::vector<std::string> realObjects; //With groups, we may have to generate expression for more than one object list.
    if ( globalGroup != game.objectGroups.end() )
        realObjects = (*globalGroup).GetAllObjectsNames();
    else if ( sceneGroup != scene.objectGroups.end() )
        realObjects = (*sceneGroup).GetAllObjectsNames();
    else
        realObjects.push_back(parameters[0].GetPlainString());

    //If current object is present, use it and only it.
    if ( find(realObjects.begin(), realObjects.end(), context.GetCurrentObject()) != realObjects.end() )
    {
        realObjects.clear();
        realObjects.push_back(context.GetCurrentObject());
    }

    std::string output = "\"\"";
    for (unsigned int i = 0;i<realObjects.size();++i)
    {
        context.ObjectsListNeeded(realObjects[i]);

        //Cast the object if needed
        string automatismType = GetTypeOfAutomatism(game, scene, parameters[1].GetPlainString());
        const AutomatismInfo & autoInfo = GDpriv::ExtensionsManager::GetInstance()->GetAutomatismInfo(automatismType);
        bool castNeeded = !autoInfo.cppClassName.empty();

        //Build string to access the automatism
        codeGenerator.AddIncludeFile(autoInfo.optionalIncludeFile);
        if ( context.GetCurrentObject() == realObjects[i] && !context.GetCurrentObject().empty() )
        {
            if ( !castNeeded )
                output = "("+ManObjListName(realObjects[i])+"[i]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\")->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(static_cast<"+autoInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[i]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\"))->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
        else
        {
            if ( !castNeeded )
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" :"+ManObjListName(realObjects[i])+"[0]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\")->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
            else
                output = "(( "+ManObjListName(realObjects[i])+".empty() ) ? "+output+" : "+"static_cast<"+autoInfo.cppClassName+"*>("+ManObjListName(realObjects[i])+"[0]->GetAutomatismRawPointer(\""+parameters[1].GetPlainString()+"\"))->"+expressionInfo.cppCallingInformation.cppCallingName+"("+parametersStr+"))";
        }
    }

    plainExpression += output;
};

bool CallbacksForGeneratingExpressionCode::OnSubMathExpression(const Game & game, const Scene & scene, GDExpression & expression)
{
    string newExpression;

    CallbacksForGeneratingExpressionCode callbacks(newExpression, game, scene, codeGenerator, context);

    GDExpressionParser parser(expression.GetPlainString());
    if ( !parser.ParseMathExpression(game, scene, callbacks) )
    {
        #if defined(GD_IDE_ONLY)
        firstErrorStr = callbacks.firstErrorStr;
        firstErrorPos = callbacks.firstErrorPos;
        #endif
        return false;
    }

    return true;
}

bool CallbacksForGeneratingExpressionCode::OnSubTextExpression(const Game & game, const Scene & scene, GDExpression & expression)
{
    string newExpression;

    CallbacksForGeneratingExpressionCode callbacks(newExpression, game, scene, codeGenerator, context);

    GDExpressionParser parser(expression.GetPlainString());
    if ( !parser.ParseTextExpression(game, scene, callbacks) )
    {
        #if defined(GD_IDE_ONLY)
        firstErrorStr = callbacks.firstErrorStr;
        firstErrorPos = callbacks.firstErrorPos;
        #endif
        return false;
    }

    return true;
}
#endif