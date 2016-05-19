
#include <iostream>

#include "errorhandler.h"
#include "scanner.h"

#include "parser.h"


parser::parser(network* netz, devices* devz, monitor* mons, scanner& scan, names* nms)
    : _netz(netz), _devz(devz), _mons(mons), _scan(scan), _nms(nms), errs(cout) {
        //_devz->debug(true);
}

parser::~parser() {
}


bool parser::readin() {
    Token tk = _scan.peek();
    parseFile(tk);

    return true;
}

void parser::stepAndPeek(Token& tk) {
    _scan.step();
    tk = _scan.peek();
}


// EBNF Rows:

// file = { statement } ;
void parser::parseFile(Token& tk) {
    while (tk.type != TokType::EndOfFile) {
        try {
            parseStatement(tk);
        }
        catch (matterror& e) {
            errs << e.what() << std::endl;
            return;
        }
    }
}


// statement = definedevice | definemonitor ;
void parser::parseStatement(Token& tk) {
    switch (tk.type) {
        case TokType::DevKeyword:
            stepAndPeek(tk);
            parseDefineDevice(tk);

            break;
        case TokType::MonitorKeyword:
            stepAndPeek(tk);
            parseDefineMonitor(tk);

            break;
        default:
            // Todo: Better message.
            throw matterror("Unexpected token type. Expected a device or monitor defintion.", _scan.getFile(), tk.at);
    }
}


// definedevice = "dev" , devicename , [ "=" , type ] , data ;
void parser::parseDefineDevice(Token& tk) {
    Token nameToken;

    if (tk.type != TokType::Identifier) {
        throw matterror("Expected a device name.", _scan.getFile(), tk.at);
    }
    // dev, as, monitor are handled by the scanner and made *Keywords
    // device types are handled by the scanner and made DeviceTypes

    nameToken = tk;
    name dv = _nms->lookup(nameToken.name);

    stepAndPeek(tk);

    if (tk.type == TokType::Equals) {
        // Semantic check: has devicename been defined before?
        if (_netz->finddevice(dv) != NULL) {
            throw matterror("Device types may not be assigned to devices that already exist.", _scan.getFile(), tk.at);
        }

        stepAndPeek(tk);

        if (tk.type != TokType::DeviceType) {
            // Todo: Suggest types
            throw matterror("Expected device type.", _scan.getFile(), tk.at);
        }

        // Create device of type, add it to the network
        devlink newDeviceLink;
        bool success;
        // setting 0 as default varient for the time being
        // TODO: this may need to be changed for switches, clocks
        _devz->makedevice(tk.devtype, dv, 0, success);

        // Debug
        /*
        std::cout << "made device ";
        _nms->writename(dv);
        std::cout << " regtype ";
        _devz->writedevice(_netz->finddevice(dv)->kind);
        //_devz->writedevice(_devz->devkind(dv));
        cout << std::endl;
        */
        if (!success) {
            throw matterror("Unable to add device.", _scan.getFile(), tk.at);
            // TODO: Better error message? Shouldn't ever reach here.
        }

        stepAndPeek(tk);
    }

    parseData(tk, dv);
}


// data = optionset | ";" ;
void parser::parseData(Token& tk, name dv) {
    if (tk.type == TokType::SemiColon) {
        stepAndPeek(tk);
    }
    else if (tk.type == TokType::Brace) {
        stepAndPeek(tk);

        parseOptionSet(tk, dv);
    }
    else {
        // Todo: Better error message.
        throw matterror("Unexpected token. Expecting ; or {.", _scan.getFile(), tk.at);
    }
}


// optionset = "{" , { option } , "}" ;
void parser::parseOptionSet(Token& tk, name dv) {
    while (tk.type != TokType::CloseBrace) {
        if (tk.type == TokType::EndOfFile) {
            throw matterror("Unterminated braces.", _scan.getFile(), tk.at);
        }

        parseOption(tk, dv);
    }

    stepAndPeek(tk);
}

bool isLegalGateInputNamestring(namestring s, int maxn) {
    // Todo: Probably a cleaner way to do this
    if (s.at(0) != 'I')
        return false;
    int val = 0;
    if (s.length() == 2 && std::isdigit(s.at(1))) {
        val = s.at(1) - '0';
    } else if (s.length() == 3
        && std::isdigit(s.at(1))
        && std::isdigit(s.at(2))){
        val = (s.at(1) - '0')*10
            + (s.at(2) - '0');
    } else {
        return false;
    }
    return (val >= 1 && val <= maxn);
}


// option = key , ":" , value , ";" ;
void parser::parseOption(Token& tk, name dv) {
    Token key, value;
    devlink dvl = _netz->finddevice(dv);

    if (tk.type != TokType::Identifier) {
        throw matterror("Expected a key.", _scan.getFile(), tk.at);
    }

    key = tk;
    name keyname = _nms->lookup(key.name);

    // Todo: Name lookup.
    // Todo: not sure why  _devz->devkind(dv)  wasn't working (always returns baddevice)
    switch(dvl->kind) {
        // Todo: quality check errors
        case aswitch:
            if (key.name != "InitialValue")
                throw matterror("Switches may only have an `InitialValue` attribute.", _scan.getFile(), key.at);
            break;
        case aclock:
            if (key.name != "Period")
                throw matterror("Clocks may only have a `Period` attribute.", _scan.getFile(), key.at);
            break;
        case andgate:
            if (!isLegalGateInputNamestring(key.name, 16))
                throw matterror("AND gates may only have input pin attributes (up to 16), labelled I1 to I16", _scan.getFile(), key.at);
            break;
        case nandgate:
            if (!isLegalGateInputNamestring(key.name, 16))
                throw matterror("NAND gates may only have input pin attributes (up to 16), labelled I1 to I16", _scan.getFile(), key.at);
            break;
        case orgate:
            if (!isLegalGateInputNamestring(key.name, 16))
                throw matterror("OR gates may only have input pin attributes (up to 16), labelled I1 to I16", _scan.getFile(), key.at);
            break;
        case norgate:
            if (!isLegalGateInputNamestring(key.name, 16))
                throw matterror("NOR gates may only have input pin attributes (up to 16), labelled I1 to I16", _scan.getFile(), key.at);
            break;
        case xorgate:
            if (!isLegalGateInputNamestring(key.name, 2))
                throw matterror("XOR gates may only have input pin attributes (up to 2), labelled I1 to I2", _scan.getFile(), key.at);
            break;
        case dtype:
            if (!(key.name == "DATA" || key.name == "CLK" || key.name == "SET" || key.name == "CLEAR"))
                throw matterror("DTYPE devices may only have DATA, CLK, SET or CLEAR input pins assigned", _scan.getFile(), key.at);
                // Todo: closest word suggestion
            break;
        case baddevice:
        default:
            // Should never reach here
            // Todo: better error message?
            throw matterror("Could not assign key to a bad device type", _scan.getFile(), key.at);
    }

    // Todo: check if key has already been defined for particular device

    stepAndPeek(tk);

    if (tk.type != TokType::Colon) {
        throw matterror("Expected colon.", _scan.getFile(), tk.at);
    }

    stepAndPeek(tk);
    value = tk;

    if (dvl->kind == aswitch) {
        // Switch
        if (value.type != TokType::Number || value.number != 0 && value.number != 1)
            throw matterror("Switches must have initial values of either 0 or 1", _scan.getFile(), value.at);
        
        asignal sig = value.number ? high : low;
        bool success = false;

        _devz->setswitch(dv, sig, success);
        if (!success)
            throw matterror("Could not set switch initial value", _scan.getFile(), value.at);

        stepAndPeek(tk);

    } else if (dvl->kind == aclock) { 
        // Clock
        if (value.type != TokType::Number || value.number < 1 || value.number >= 32767)
            throw matterror("Clock periods must be integers between 1 and 32767", _scan.getFile(), value.at);
        dvl->frequency = value.number;

        stepAndPeek(tk);

    } else {
        // A gate
        Signal sig = parseSignalName(tk);
        // connect the gate
        _netz->addinput(dvl, keyname);
        bool success = false;
        _netz->makeconnection(dv, keyname, sig.device, sig.pin, success);
        if (!success)
            // Todo: improve error message
            throw matterror("Could not make connection", _scan.getFile(), key.at);

    }

    if (tk.type != TokType::SemiColon) {
        throw matterror("Missing a semicolon on the end.", _scan.getFile(), tk.at);
    }
    stepAndPeek(tk);
}


// definemonitor = "monitor" , monitorset , ";" ;
void parser::parseDefineMonitor(Token& tk) {
    for (;;) {
        parseMonitor(tk);

        if (tk.type == TokType::SemiColon) {
            stepAndPeek(tk);
            break;
        }
        else if (tk.type != TokType::Comma) {
            throw matterror("Expected a comma in the monitor list.", _scan.getFile(), tk.at);
        }
        stepAndPeek(tk);
    }
}


// monitor = signalname , [ "as" , signalname ] ;
void parser::parseMonitor(Token& tk) {

    Signal sig = parseSignalName(tk);

    if (tk.type == TokType::AsKeyword) {
        stepAndPeek(tk);

        //Signal alias = parseSignalName(tk);
        // Todo: parse alias
        stepAndPeek(tk);

        // Todo: Warn if alias already used.
    }

    bool success = false;
    _mons->makemonitor(sig.device, sig.pin, success);
    if (!success)
        // Todo: improve error message
        throw matterror("Could not make monitor", _scan.getFile(), tk.at);
}


// signalname = devicename , [ "." , pin ] ;
Signal parser::parseSignalName(Token& tk) {

    Signal ret;
    ret.device = blankname;
    ret.pin = blankname;

    if (tk.type != TokType::Identifier) {
        throw matterror("Expected a signal name.", _scan.getFile(), tk.at);
    }

    // Todo: handle 0 or 1 connections
    ret.device = _nms->lookup(tk.name);

    // Ensure device exists
    if (_netz->finddevice(ret.device) == NULL) {
        throw matterror("Devices must be defined before being referenced", _scan.getFile(), tk.at);
    }

    stepAndPeek(tk);

    if (tk.type == TokType::Dot) {
        stepAndPeek(tk);

        if (tk.type != TokType::Identifier) {
            throw matterror("Expected a pin name.", _scan.getFile(), tk.at);
        }

        ret.pin = _nms->lookup(tk.name);
        // Todo: ensure pin is acceptable identifier
        stepAndPeek(tk);
    }

    return ret;
}
