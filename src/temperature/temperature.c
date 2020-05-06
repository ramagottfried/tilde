
#define NAME "temperature"
#define DESCRIPTION "report current computer temperature"
#define AUTHORS "Rama Gottfried"
#define COPYRIGHT_YEARS "2018"

#ifdef PD_VERION
#include "m_pd.h"
#else
#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#endif


#include "osc.h"
#include "osc_mem.h"
#include "omax_util.h"
#include "omax_doc.h"
#include "o.h"

#include <IOKit/IOKitLib.h>

#include "smc.h"

t_symbol *ps_FullPacket;

static t_class *temperature_class;

typedef struct _temperature {
    t_object  ob;
    void *outlet;
    char **fanIDS;
    int nfans;
    
    io_connect_t conn;
} t_temperature;





UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;
    
    for (i = 0; i < size; i++)
    {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
            total += (unsigned char) (str[i] << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c",
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

kern_return_t SMCOpen(t_temperature *x)
{
    kern_return_t result;
    io_iterator_t iterator;
    io_object_t   device;
    
    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }
    
    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
    {
        printf("Error: no SMC found\n");
        return 1;
    }
    
    result = IOServiceOpen(device, mach_task_self(), 0, &x->conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }
    
    return kIOReturnSuccess;
}

kern_return_t SMCClose(t_temperature *x)
{
    return IOServiceClose(x->conn);
}


kern_return_t SMCCall(t_temperature *x, int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;
    
    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);
    
#if MAC_OS_X_VERSION_10_5
    return IOConnectCallStructMethod( x->conn, index,
                                     // inputStructure
                                     inputStructure, structureInputSize,
                                     // ouputStructure
                                     outputStructure, &structureOutputSize );
#else
    return IOConnectMethodStructureIStructureO( x->conn, index,
                                               structureInputSize, /* structureInputSize */
                                               &structureOutputSize,   /* structureOutputSize */
                                               inputStructure,        /* inputStructure */
                                               outputStructure);       /* ouputStructure */
#endif
    
}

kern_return_t SMCReadKey(t_temperature *x, UInt32Char_t key, SMCVal_t *val)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));
    
    inputStructure.key = _strtoul(key, 4, 16);
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;
    
    result = SMCCall(x, KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;
    
    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;
    
    result = SMCCall(x, KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;
    
    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));
    
    return kIOReturnSuccess;
}


double SMCGetTemperature(t_temperature *x, char *key)
{
    SMCVal_t val;
    kern_return_t result;
    
    result = SMCReadKey(x, key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
                // convert sp78 value to temperature
                int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
                return intValue / 256.0;
            }
        }
    }
    // read failed
    return 0.0;
}

double SMCGetFanSpeed(t_temperature *x, char *key)
{
    SMCVal_t val;
    kern_return_t result;
    
    result = SMCReadKey(x, key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert fpe2 value to rpm
                int intValue = (unsigned char)val.bytes[0] * 256 + (unsigned char)val.bytes[1];
                return intValue / 4.0;
            }
        }
    }
    // read failed
    return 0.0;
}


double convertToFahrenheit(double celsius) {
    return (celsius * (9.0 / 5.0)) + 32.0;
}

float SMCGetFanRPM(t_temperature *x, char *key)
{
    SMCVal_t val;
    kern_return_t result;
    
    result = SMCReadKey(x, key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert fpe2 value to RPM
                return ntohs(*(UInt16*)val.bytes) / 4.0;
            }
        }
    }
    // read failed
    return -1.f;
}

void temperature_getFans(t_temperature *x, t_osc_bndl_u *bndl)
{
    kern_return_t result;
    SMCVal_t val;
    UInt32Char_t key;
    int totalFans, i;
    
    result = SMCReadKey(x, "FNum", &val);
    
    if(result == kIOReturnSuccess)
    {
        totalFans = _strtoul((char *)val.bytes, val.dataSize, 10);
        
        printf("Num fans: %d\n", totalFans);
        for(i = 0; i < totalFans; i++) {
            sprintf(key, "F%dID", i);
            result = SMCReadKey(x, key, &val);
            if (result != kIOReturnSuccess) {
                continue;
            }
            char* name = val.bytes + 4;
            
            sprintf(key, "F%dAc", i);
            float actual_speed = SMCGetFanRPM(x, key);
            if (actual_speed < 0.f) {
                continue;
            }
            
            sprintf(key, "F%dMn", i);
            float minimum_speed = SMCGetFanRPM(x, key);
            if (minimum_speed < 0.f) {
                continue;
            }
            
            sprintf(key, "F%dMx", i);
            float maximum_speed = SMCGetFanRPM(x, key);
            if (maximum_speed < 0.f) {
                continue;
            }
            
            float rpm = actual_speed - minimum_speed;
            if (rpm < 0.f) {
                rpm = 0.f;
            }
            float pct = rpm / (maximum_speed - minimum_speed);
            
            pct *= 100.f;
            printf("Fan %d - %s at %.0f RPM (%.0f%%)\n", i, name, rpm, pct);
            
            //sprintf(key, "F%dSf", i);
            //SMCReadKey(key, &val);
            //printf("    Safe speed   : %.0f\n", strtof(val.bytes, val.dataSize, 2));
            //sprintf(key, "F%dTg", i);
            //SMCReadKey(key, &val);
            //printf("    Target speed : %.0f\n", strtof(val.bytes, val.dataSize, 2));
            //SMCReadKey("FS! ", &val);
            //if ((_strtoul((char *)val.bytes, 2, 16) & (1 << i)) == 0)
            //    printf("    Mode         : auto\n");
            //else
            //    printf("    Mode         : forced\n");
        }
    }
    
}

void temperature_poll(t_temperature *x)
{

    double cpu = SMCGetTemperature(x, SMC_KEY_CPU_TEMP);
    double gpu = SMCGetTemperature(x, SMC_KEY_GPU_TEMP);

    t_osc_bndl_u *bndl = osc_bundle_u_alloc();
    
    
    
    t_osc_msg_u *msg = osc_message_u_allocWithAddress("/temperature/cpu");
    osc_message_u_appendDouble(msg, cpu);
    osc_bundle_u_addMsg(bndl, msg);
    
    msg = osc_message_u_allocWithAddress("/temperature/gpu");
    osc_message_u_appendDouble(msg, gpu);
    osc_bundle_u_addMsg(bndl, msg);
    
    t_osc_bndl_s *s_bndl = osc_bundle_u_serialize(bndl);
    omax_util_outletOSC(x->outlet, osc_bundle_s_getLen(s_bndl), osc_bundle_s_getPtr(s_bndl) );
    
    if( s_bndl )
        osc_bundle_s_deepFree(s_bndl);
    
    osc_bundle_u_free(bndl);
    
}

SMCKeyData_t temperature_getIOKey(char *key)
{
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    
    inputStructure.key = _strtoul(key, 4, 16);
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;
    return inputStructure;
}

void temperature_free(t_temperature *x)
{
    SMCClose(x);
}

void *temperature_new(t_symbol *s, long argc, t_atom *argv)
{
    t_temperature *x = (t_temperature *)object_alloc(temperature_class);
    
    if (x)
    {
        SMCOpen(x);
        
        // to do: pregenerate keys so we don't do it on every tick
        
        
        
        x->outlet = outlet_new(&x->ob, NULL);
    }
    return (x);
}

int main(void)
{
    t_class *c = class_new(NAME, (method)temperature_new, (method)temperature_free, (long)sizeof(t_temperature), NULL, A_GIMME);
    
    class_addmethod(c, (method)temperature_poll,   "bang",    A_CANT, 0);
    
    class_register(CLASS_BOX, c);
    temperature_class = c;
    
    post("%s by %s.", NAME, AUTHORS);
    post("Copyright (c) " COPYRIGHT_YEARS " HfMT-Hamburg.  All rights reserved.");
    
    return 0;
    
}

