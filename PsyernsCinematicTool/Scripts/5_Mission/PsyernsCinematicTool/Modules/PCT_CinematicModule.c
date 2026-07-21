class PCT_CinematicModule: JMRenderableModuleBase
{
	void PCT_CinematicModule() {
		GetPermissionsManager().RegisterPermission( "CinematicTool.View" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Sequencer" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.World" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Actors" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Lights" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Props" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Export" );
	}

	override bool HasAccess() {
		return GetPermissionsManager().HasPermission( "CinematicTool.View" );
	}

	override string GetInputToggle() {
		return "UAPCT_ToggleMenu";
	}

	override string GetLayoutRoot() {
		return "PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout";
	}

	override string GetTitle() {
		return "Psyerns Cinematic Tool";
	}

	override string GetIconName() {
		return "C";
	}

	override bool ImageIsIcon() {
		return false;
	}

	override string GetWebhookTitle() {
		return "Psyerns Cinematic Tool";
	}

	override int GetRPCMin() {
		return EPCT_RPC.INVALID;
	}

	override int GetRPCMax() {
		return EPCT_RPC.COUNT;
	}
}
