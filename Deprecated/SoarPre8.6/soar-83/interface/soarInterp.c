/*
 * =======================================================================
 *  File:  soarInterp.c
 *
 * Routines to handle the interpreter apect of agents.
 *
 * =======================================================================
 *
 *
 * Copyright 1995-2003 Carnegie Mellon University,
 *										 University of Michigan,
 *										 University of Southern California/Information
 *										 Sciences Institute. All rights reserved.
 *										
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.	Redistributions of source code must retain the above copyright notice,
 *		this list of conditions and the following disclaimer. 
 * 2.	Redistributions in binary form must reproduce the above copyright notice,
 *		this list of conditions and the following disclaimer in the documentation
 *		and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE SOAR CONSORTIUM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE SOAR CONSORTIUM  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Carnegie Mellon University, the
 * University of Michigan, the University of Southern California/Information
 * Sciences Institute, or the Soar consortium.
 * =======================================================================
 */

#include <string.h>
#include <stdlib.h>

#include "soar.h"
#include "soarScheduler.h"
#include "soarCommandUtils.h"

/* Globals vars (with respect to this file) */

RegisteredInterp *registry = NULL;  /* List of all interpreters
				     * registered by this process. */




/*
 *--------------------------------------------------------------
 *
 * Soar_SetSoarLibrary
 *
 *	This procedure is called to set the "soar_library"
 *      Tcl variable.
 *
 * Results:
 *	TCL_OK if successful.
 *
 * Side effects:
 *	The soar_library global variable will exist and be
 *	set to the appropriate location.
 *
 *--------------------------------------------------------------
 */

/* This little Tcl code fragment tries to use the SOAR_LIBRARY
   environment variable first, then searches each directory in the
   tcl_pkgPath and auto_path variables for a "soar" subdirectory. The
   first one it finds (if any) is made into soar_library. */

static char setSoarLibraryCommand[] =
"set soar_library {}\n\
if {[info exists env(SOAR_LIBRARY)]} {\n\
	set soar_library $env(SOAR_LIBRARY)\n\
} else {\n\
	foreach dir [concat $tcl_pkgPath $auto_path] {\n\
		if {[file exists [file join $dir soar]]} {\n\
			set soar_library [file join $dir soar]\n\
			break\n\
		}\n\
	}\n\
}";


static int
Soar_SetSoarLibrary(Tcl_Interp *interp)
{
	return Tcl_GlobalEval(interp, setSoarLibraryCommand);
}




/*
 *--------------------------------------------------------------
 *
 * Soar_ReadInitScript
 *
 *	Sources the Soar init script, $soar_library/soar.tcl.
 *
 * Results:
 *	TCL_OK if successful.
 *
 * Side effects:
 *	Arbitrary, depending on the contents of soar.tcl.
 *
 *--------------------------------------------------------------
 */

/* This code fragment tries to find "soar.tcl" using the $soar_library
   environment variable. For some inane reason, I had to use [set
   soar_library] rather than simple $soar_library to avoid getting
   segfaults. Don't ask me why. I just work here. */

static char readInitScriptCommand[] =
"if [file exists [file join [set soar_library] soar.tcl]] {\n\
	source [file join $soar_library soar.tcl]\n\
} else {\n\
	puts \"Warning! can't find [file join $soar_library soar.tcl]; perhaps you need to\"\n\
	puts \"re-install Soar or set your SOAR_LIBRARY environment variable?\"\n\
}";


static int
Soar_ReadInitScript(Tcl_Interp *interp)
{
		return Tcl_GlobalEval(interp, readInitScriptCommand); 
}





/*
 *--------------------------------------------------------------
 *
 * Soar_MakeRegisterEntry --
 *
 *	This procedure is called to create a new register
 *      entry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Registration info is saved, thereby allowing the
 *	"send" command to be used later to invoke commands
 *	in the interpreter.  The registration will be removed
 *	automatically when the interpreter is deleted.
 *
 *--------------------------------------------------------------
 */

RegisteredInterp *
Soar_MakeRegisterEntry (interp, agentPtr)
    Tcl_Interp *interp;		/* Interpreter associated with the agent. */
    agent * agentPtr;    
{
    RegisteredInterp *riPtr;

    /*
     * Add an entry in the local registry of names owned by this
     * process.
     */

    riPtr = (RegisteredInterp *) ckalloc(sizeof(RegisteredInterp));

    riPtr->interp   = interp;
    riPtr->agentPtr = agentPtr;
    riPtr->nextPtr  = registry;
    registry = riPtr;

    return riPtr;
  }

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SelectGlobalInterpByInterp --
 *
 *	Select an interpreter to use at the top level.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the global interpreter.
 *
 *----------------------------------------------------------------------
 */

void 
Soar_SelectGlobalInterpByInterp (interp)
    Tcl_Interp * interp;        /* interpreter to use at the top-level. */
{
  RegisteredInterp *riPtr;

  for (riPtr = registry; riPtr != NULL; riPtr = riPtr->nextPtr) {
    if (riPtr->interp == interp) {
      mainInterp = riPtr;
      interp     = riPtr->interp;
      soar_agent = riPtr->agentPtr;
    }
  }
}


/*
 *--------------------------------------------------------------
 *
 * Soar_GetRegisteredInterpByInterp --
 *
 *	This procedure is called to get a registered interp by
 *      its Tcl interp struct.
 *
 * Results:
 *	NULL if the name is not in the registry, a pointer to
 *      the registered interp if it is.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

RegisteredInterp * 
Soar_GetRegisteredInterpByInterp (interp)
     Tcl_Interp * interp;
{
  RegisteredInterp *riPtr;

  for (riPtr = registry; riPtr != NULL; riPtr = riPtr->nextPtr) {
    if (interp == riPtr->interp) {
      return riPtr;
    }
  }
  
  return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * Soar_DestroyRegisterEntry --
 *
 *	This procedure is called to remove a registered interp.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the indicated entry, if present.
 *
 *--------------------------------------------------------------
 */

void
Soar_DestroyRegisterEntry (riPtrToRemove)
  RegisteredInterp *riPtrToRemove;
{
  RegisteredInterp *riPtr;
  RegisteredInterp *riPtrPrev = NULL;

  for (riPtr = registry; riPtr != NULL; riPtr = riPtr->nextPtr) {
    if (riPtr == riPtrToRemove) {
      if (riPtr == registry)
	{
	  registry = riPtr->nextPtr;
	}
      else
	{
	  riPtrPrev->nextPtr = riPtr->nextPtr;
	}
      ckfree((char *) riPtr);
    }
    riPtrPrev = riPtr;
  }
}


/*
 *----------------------------------------------------------------------
 *
 * SoarnewsCmd --
 *
 *      This is the command procedure for the "soarnews" command, 
 *      which prints out information about the current Soar release.
 *
 * Syntax:  soarnews
 *
 * Results:
 *      Returns a standard Tcl completion code.
 *
 * Side effects:
 *      Prints the latest Soar news.
 *
 *----------------------------------------------------------------------
 */

int SoarnewsCmd (ClientData clientData, 
		 Tcl_Interp * interp,
		 int argc, char *argv[])
{
  /* BUGBUG update soarnews printout on successive versions */

  Tcl_AppendResult(interp,
		   "News for Soar version ", soar_version_string, ":\n", 
		   soar_news_string, "\n", (char *) NULL);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VersionCmd --
 *
 *      This is the command procedure for the "version" command, 
 *      which prints information about the current Soar version
 *      being run.
 *
 * Syntax:  version
 *
 * Results:
 *      Returns a standard Tcl completion code.
 *
 * Side effects:
 *      Returns the version number in the interpreter result.
 *
 *----------------------------------------------------------------------
 */
int VersionCmd (ClientData clientData, 
		Tcl_Interp * interp,
		int argc, char *argv[])
{
  if (argc > 1)
    {
      interp->result = "Too many arguments, should be: version";
      return TCL_ERROR;
    }

  sprintf(interp->result, "%s", soar_version_string); 
  return TCL_OK;
}



/*
 *--------------------------------------------------------------
 *
 * Soar_InterpDeletProc --
 *
 *	Called immediately before the interpreter is deleted. Destroys
 *	the agent and removes it from the registry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The registry is modified.
 *
 *--------------------------------------------------------------
 */

static void
Soar_InterpDeleteProc(ClientData clientData, Tcl_Interp *interp)
{
	RegisteredInterp* riPtr;
	
	if (riPtr = Soar_GetRegisteredInterpByInterp(interp)) {
		if (riPtr->agentPtr)
			destroy_soar_agent(riPtr->agentPtr);

		Soar_DestroyRegisterEntry(riPtr);
	}
}




/*
 *--------------------------------------------------------------
 *
 * Soar_Init --
 *
 *	Called each time that Soar is loaded into an interpreter. Sets
 *	the soar_library Tcl variable, sources the init script, and
 *	installs the Soar commands.
 *
 * Results:
 *	TCL_OK if everything is successful.
 *
 * Side effects:
 *
 *	The registry is modified, any additional side effects due to the
 *	initialization script.
 *
 *--------------------------------------------------------------
 */

#if defined(WIN32)
__declspec(dllexport)
#endif /* WIN32 */
int
Soar_Init(Tcl_Interp* interp)
{
  /* char szBuffer[] = "tk_messageBox -type ok -message \"Soar_Init called\" -icon info";  */
	char name[32];
	agent *new_agent;


#if !defined(WIN32)
	static int soar_init_invoked = 0;

	/* In WIN32, this is done in DllMain. Maybe there's a better
           way to do it in UNIX */
	if (! soar_init_invoked) {
		soar_init_invoked = 1;
		init_soar();
	}
#endif


	/* Tcl_Eval(interp, szBuffer); */
	/* Export the Soar package */
	Tcl_PkgProvide(interp, "Soar", soar_version_string);


	/* Create an agent for the interpreter */
	sprintf(name, "[Agent:0x%lx]", interp);

	new_agent = create_soar_agent (name);
	new_agent->interpreter = interp;
	new_agent->tcl_output_buffer = NULL;


	/* Install the Soar commands */
	Soar_InstallCommands(new_agent);
	Tcl_CreateCommand(interp, "soarnews", SoarnewsCmd, NULL, NULL);
	Tcl_CreateCommand(interp, "version", VersionCmd, NULL, NULL);

	Soar_LinkInterpVars2Agent(interp, new_agent);


#ifdef TCL_MEM_DEBUG
	Tcl_InitMemory(interp);
#endif

#if !defined(WIN32) && !defined(MACINTOSH)
	Tcl_SetVar(interp, "tcl_man_dir", TCL_MAN_DIR, TCL_GLOBAL_ONLY);
#endif


	Soar_MakeRegisterEntry(interp, new_agent);
	Tcl_CallWhenDeleted(interp, Soar_InterpDeleteProc, NULL);

	if (Soar_SetSoarLibrary(interp) != TCL_OK)
		return TCL_ERROR;

	if (Soar_ReadInitScript(interp) != TCL_OK)
		return TCL_ERROR;

	return TCL_OK;
}

