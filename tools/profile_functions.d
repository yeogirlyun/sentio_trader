#!/usr/sbin/dtrace -s

#pragma D option quiet

BEGIN {
    printf("🔍 PROFILING SENTIO FUNCTION CALLS\n");
    printf("===================================\n");
    start_time = timestamp;
}

/* Track function entry and exit for sentio binary */
pid$target::*:entry
/execname == "sentio_cli"/
{
    @functions[probefunc] = count();
    @modules[probemod] = count();
}

/* Track library function calls */
pid$target:*:*:entry
/execname == "sentio_cli" && probefunc != ""/
{
    @lib_functions[strjoin(probemod, "::", probefunc)] = count();
}

END {
    printf("\n📊 FUNCTION CALL ANALYSIS\n");
    printf("=========================\n");
    
    printf("\n🔥 TOP 20 MOST CALLED FUNCTIONS:\n");
    printf("%-50s %s\n", "Function", "Call Count");
    printf("%-50s %s\n", "--------", "----------");
    trunc(@functions, 20);
    printa("%-50s %@d\n", @functions);
    
    printf("\n📚 MODULE USAGE:\n");
    printf("%-30s %s\n", "Module", "Call Count");
    printf("%-30s %s\n", "------", "----------");
    printa("%-30s %@d\n", @modules);
    
    printf("\n📖 LIBRARY FUNCTION CALLS (Top 10):\n");
    printf("%-60s %s\n", "Library::Function", "Call Count");
    printf("%-60s %s\n", "-----------------", "----------");
    trunc(@lib_functions, 10);
    printa("%-60s %@d\n", @lib_functions);
    
    elapsed = (timestamp - start_time) / 1000000; /* Convert to milliseconds */
    printf("\n⏱️  Total execution time: %d ms\n", elapsed);
}
