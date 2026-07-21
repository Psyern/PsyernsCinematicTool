class CfgPatches
{
	class PCT_Scripts
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] =
		{
			"DZ_Data",
			"JM_CF_Scripts",
			"JM_COT_Scripts",
			"JM_COT_GUI"
		};
	};
};

class CfgMods
{
	class PsyernsCinematicTool
	{
		dir = "PsyernsCinematicTool";
		name = "Psyerns Cinematic Tool";
		author = "Psyern";
		credits = "Psyern";
		version = "0.1.0";
		extra = 0;
		type = "mod";
		inputs = "PsyernsCinematicTool\Scripts\Data\Inputs.xml";

		dependencies[] =
		{
			"Game",
			"World",
			"Mission"
		};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\3_Game"
				};
			};
			class worldScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\4_World"
				};
			};
			class missionScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\5_Mission"
				};
			};
		};
	};
};
