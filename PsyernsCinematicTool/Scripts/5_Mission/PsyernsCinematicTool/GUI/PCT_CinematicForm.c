class PCT_CinematicForm: JMFormBase
{
	protected PCT_CinematicModule m_Module;

	protected override bool SetModule( JMRenderableModuleBase mdl ) {
		return Class.CastTo( m_Module, mdl );
	}

	override void OnInit() {
	}

	override void OnShow() {
		super.OnShow();
	}

	override void OnHide() {
		super.OnHide();
	}

	override void OnSettingsUpdated() {
	}
}
