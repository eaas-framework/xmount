﻿REM VB.Net program to glob Expert Witness Compression Format file(s) using ewf.net
REM
REM Author:            Joachim Metz
REM Creation date:     November 3, 2010
REM Modification date: July 24, 2011

Imports EWF

Module Glob

Function Main(ByVal args() As String) As Integer
        Dim handle As New EWF.Handle 
        Dim ewf As New EWF.EWF   

	Dim filenames As Array = Nothing
	Dim filename As String = Nothing
	Dim program As String  = "Glob.exe"

	Console.WriteLine(program + " (ewf.net " + ewf.GetVersion() + ")")

	If args.Length() <> 1 Then
		Console.WriteLine("Usage: " + program + " filename")

		Return 1
	End If

	filenames = handle.Glob(args(0))

	If filenames.Length() > 0 Then
		Console.WriteLine("Filenames:")

		For Each filename In filenames
			Console.WriteLine(filename)
		Next
	End If

	Console.WriteLine("")

	Return 0
End Function

End Module

