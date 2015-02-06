<?xml version='1.0'?>
<Project Type="Project" LVVersion="8508002">
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="lib" Type="Folder">
			<Item Name="asIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/asIoc.dll"/>
			<Item Name="ca.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/ca.dll"/>
			<Item Name="caLab.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/caLab.dll"/>
			<Item Name="caLabIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/caLabIoc.dll"/>
			<Item Name="Com.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/Com.dll"/>
			<Item Name="dbIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/dbIoc.dll"/>
			<Item Name="dbStaticIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/dbStaticIoc.dll"/>
			<Item Name="dbtoolsIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/dbtoolsIoc.dll"/>
			<Item Name="miscIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/miscIoc.dll"/>
			<Item Name="msvcp100.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/msvcp100.dll"/>
			<Item Name="msvcr100.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/msvcr100.dll"/>
			<Item Name="recIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/recIoc.dll"/>
			<Item Name="registryIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/registryIoc.dll"/>
			<Item Name="rsrvIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/rsrvIoc.dll"/>
			<Item Name="softDevIoc.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/softDevIoc.dll"/>
			<Item Name="softIoc.dbd" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/softIoc.dbd"/>
			<Item Name="softIoc.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/lib/softIoc.exe"/>
		</Item>
		<Item Name="SoftIOC Demo.vi" Type="VI" URL="SoftIOC Demo.vi"/>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="user.lib" Type="Folder">
				<Item Name="CaLabSoftIOC.vi" Type="VI" URL="../CaLabSoftIOC.vi"/>
				<Item Name="SoftIocPathName.vi" Type="VI" URL="../Private/SoftIocPathName.vi"/>
				<Item Name="DbdPathName.vi" Type="VI" URL="../Private/DbdPathName.vi"/>
				<Item Name="CheckWindows.vi" Type="VI" URL="../Private/CheckWindows.vi"/>
				<Item Name="ioc_config.vi" Type="VI" URL="../Private/ioc_config.vi"/>
				<Item Name="PV.ctl" Type="VI" URL="../PV.ctl"/>
				<Item Name="CaLabPut.vi" Type="VI" URL="../CaLabPut.vi"/>
				<Item Name="Put_Boolean-1D_PV-1D.vi" Type="VI" URL="../Private/Put_Boolean-1D_PV-1D.vi"/>
				<Item Name="CaLabPut_Main.vi" Type="VI" URL="../Private/CaLabPut_Main.vi"/>
				<Item Name="Put_Boolean-2D_PV-1D.vi" Type="VI" URL="../Private/Put_Boolean-2D_PV-1D.vi"/>
				<Item Name="Put_DBL_PV.vi" Type="VI" URL="../Private/Put_DBL_PV.vi"/>
				<Item Name="Put_DBL-1D_PV-1D.vi" Type="VI" URL="../Private/Put_DBL-1D_PV-1D.vi"/>
				<Item Name="Put_DBL-2D_PV-1D.vi" Type="VI" URL="../Private/Put_DBL-2D_PV-1D.vi"/>
				<Item Name="Put_I8_PV.vi" Type="VI" URL="../Private/Put_I8_PV.vi"/>
				<Item Name="Put_I8-1D_PV-1D.vi" Type="VI" URL="../Private/Put_I8-1D_PV-1D.vi"/>
				<Item Name="Put_I8-2D_PV-1D.vi" Type="VI" URL="../Private/Put_I8-2D_PV-1D.vi"/>
				<Item Name="Put_I16_PV.vi" Type="VI" URL="../Private/Put_I16_PV.vi"/>
				<Item Name="Put_I16-1D_PV-1D.vi" Type="VI" URL="../Private/Put_I16-1D_PV-1D.vi"/>
				<Item Name="Put_I16-2D_PV-1D.vi" Type="VI" URL="../Private/Put_I16-2D_PV-1D.vi"/>
				<Item Name="Put_I32_PV.vi" Type="VI" URL="../Private/Put_I32_PV.vi"/>
				<Item Name="Put_I32-1D_PV-1D.vi" Type="VI" URL="../Private/Put_I32-1D_PV-1D.vi"/>
				<Item Name="Put_I32-2D_PV-1D.vi" Type="VI" URL="../Private/Put_I32-2D_PV-1D.vi"/>
				<Item Name="Put_I64_PV.vi" Type="VI" URL="../Private/Put_I64_PV.vi"/>
				<Item Name="Put_I64-1D_PV-1D.vi" Type="VI" URL="../Private/Put_I64-1D_PV-1D.vi"/>
				<Item Name="Put_I64-2D_PV-1D.vi" Type="VI" URL="../Private/Put_I64-2D_PV-1D.vi"/>
				<Item Name="Put_SGL_PV.vi" Type="VI" URL="../Private/Put_SGL_PV.vi"/>
				<Item Name="Put_SGL-1D_PV-1D.vi" Type="VI" URL="../Private/Put_SGL-1D_PV-1D.vi"/>
				<Item Name="Put_SGL-2D_PV-1D.vi" Type="VI" URL="../Private/Put_SGL-2D_PV-1D.vi"/>
				<Item Name="Put_String_PV.vi" Type="VI" URL="../Private/Put_String_PV.vi"/>
				<Item Name="Put_String-1D_PV-1D.vi" Type="VI" URL="../Private/Put_String-1D_PV-1D.vi"/>
				<Item Name="Put_String-2D_PV-1D.vi" Type="VI" URL="../Private/Put_String-2D_PV-1D.vi"/>
				<Item Name="Put_Boolean_PV.vi" Type="VI" URL="../Private/Put_Boolean_PV.vi"/>
				<Item Name="Put_Boolean-1D_PV.vi" Type="VI" URL="../Private/Put_Boolean-1D_PV.vi"/>
				<Item Name="Put_String-1D_PV.vi" Type="VI" URL="../Private/Put_String-1D_PV.vi"/>
				<Item Name="Put_DBL-1D_PV.vi" Type="VI" URL="../Private/Put_DBL-1D_PV.vi"/>
				<Item Name="Put_I8-1D_PV.vi" Type="VI" URL="../Private/Put_I8-1D_PV.vi"/>
				<Item Name="Put_I16-1D_PV.vi" Type="VI" URL="../Private/Put_I16-1D_PV.vi"/>
				<Item Name="Put_I64-1D_PV.vi" Type="VI" URL="../Private/Put_I64-1D_PV.vi"/>
				<Item Name="Put_SGL-1D_PV.vi" Type="VI" URL="../Private/Put_SGL-1D_PV.vi"/>
				<Item Name="Put_I32-1D_PV.vi" Type="VI" URL="../Private/Put_I32-1D_PV.vi"/>
				<Item Name="SoftIOC Demo Sub.vi" Type="VI" URL="SoftIOC Demo Sub.vi"/>
				<Item Name="CaLabEvent.vi" Type="VI" URL="../CaLabEvent.vi"/>
				<Item Name="Get_PV-1D.vi" Type="VI" URL="../Private/Get_PV-1D.vi"/>
				<Item Name="CaLabGet_Main.vi" Type="VI" URL="../Private/CaLabGet_Main.vi"/>
				<Item Name="PV Info.ctl" Type="VI" URL="../PV Info.ctl"/>
				<Item Name="CaLabGet.vi" Type="VI" URL="../CaLabGet.vi"/>
				<Item Name="Get_PV.vi" Type="VI" URL="../Private/Get_PV.vi"/>
				<Item Name="ConfigurationSet.vi" Type="VI" URL="../Private/ConfigurationSet.vi"/>
				<Item Name="ioc_mbbi_config.vi" Type="VI" URL="../Private/ioc_mbbi_config.vi"/>
				<Item Name="CaLabInfo.vi" Type="VI" URL="../CaLabInfo.vi"/>
				<Item Name="DbPathName.vi" Type="VI" URL="../Private/DbPathName.vi"/>
			</Item>
			<Item Name="vi.lib" Type="Folder">
				<Item Name="Check if File or Folder Exists.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/libraryn.llb/Check if File or Folder Exists.vi"/>
				<Item Name="NI_FileType.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/lvfile.llb/NI_FileType.lvlib"/>
				<Item Name="subFile Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/express/express input/FileDialogBlock.llb/subFile Dialog.vi"/>
				<Item Name="ex_CorrectErrorChain.vi" Type="VI" URL="/&lt;vilib&gt;/express/express shared/ex_CorrectErrorChain.vi"/>
				<Item Name="Simple Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Simple Error Handler.vi"/>
				<Item Name="General Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler.vi"/>
				<Item Name="General Error Handler CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler CORE.vi"/>
				<Item Name="Check Special Tags.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Check Special Tags.vi"/>
				<Item Name="TagReturnType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/TagReturnType.ctl"/>
				<Item Name="Set String Value.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set String Value.vi"/>
				<Item Name="GetRTHostConnectedProp.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetRTHostConnectedProp.vi"/>
				<Item Name="Error Code Database.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Code Database.vi"/>
				<Item Name="Trim Whitespace.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Trim Whitespace.vi"/>
				<Item Name="whitespace.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/whitespace.ctl"/>
				<Item Name="Format Message String.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Format Message String.vi"/>
				<Item Name="Find Tag.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Find Tag.vi"/>
				<Item Name="Search and Replace Pattern.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Search and Replace Pattern.vi"/>
				<Item Name="Set Bold Text.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set Bold Text.vi"/>
				<Item Name="Details Display Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Details Display Dialog.vi"/>
				<Item Name="Clear Errors.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Clear Errors.vi"/>
				<Item Name="DialogTypeEnum.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogTypeEnum.ctl"/>
				<Item Name="ErrWarn.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/ErrWarn.ctl"/>
				<Item Name="eventvkey.ctl" Type="VI" URL="/&lt;vilib&gt;/event_ctls.llb/eventvkey.ctl"/>
				<Item Name="Not Found Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Not Found Dialog.vi"/>
				<Item Name="Three Button Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog.vi"/>
				<Item Name="Three Button Dialog CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog CORE.vi"/>
				<Item Name="Longest Line Length in Pixels.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Longest Line Length in Pixels.vi"/>
				<Item Name="Convert property node font to graphics font.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Convert property node font to graphics font.vi"/>
				<Item Name="Get Text Rect.vi" Type="VI" URL="/&lt;vilib&gt;/picture/picture.llb/Get Text Rect.vi"/>
				<Item Name="Get String Text Bounds.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Get String Text Bounds.vi"/>
				<Item Name="LVBoundsTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVBoundsTypeDef.ctl"/>
				<Item Name="BuildHelpPath.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/BuildHelpPath.vi"/>
				<Item Name="GetHelpDir.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetHelpDir.vi"/>
				<Item Name="DialogType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogType.ctl"/>
				<Item Name="System Exec.vi" Type="VI" URL="/&lt;vilib&gt;/Platform/system.llb/System Exec.vi"/>
				<Item Name="Merge Errors.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Merge Errors.vi"/>
				<Item Name="Error Cluster From Error Code.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Cluster From Error Code.vi"/>
			</Item>
		</Item>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
