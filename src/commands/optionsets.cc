/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/
#include "optionsets.h"
#include "utils/flags/flagtypes.h"
#include "global-settings.h"
#include "Zypper.h"

#include <sstream>

std::vector<ZyppFlags::CommandGroup> DryRunOptionSet::options()
{
  return {{{
    { "dry-run", 'D', ZyppFlags::NoArgument, ZyppFlags::BoolType( &DryRunSettings::instanceNoConst()._enabled, ZyppFlags::StoreTrue, DryRunSettings::instance()._enabled ),
         _("Don't change anything, just report what would be done.")}
  }}};
}

void DryRunOptionSet::reset()
{
  DryRunSettings::reset();
}

void InitReposOptionSet::setCompatibilityMode( CompatModeFlags flags_r )
{
  _compatMode = flags_r;
}

std::vector<ZyppFlags::CommandGroup> InitReposOptionSet::options()
{
  std::vector<ZyppFlags::CommandGroup> myOpts;
  if ( _compatMode.testFlag( CompatModeBits::EnableRugOpt ) ) {
    myOpts.push_back( {{
            { "catalog", 'c',
                  ZyppFlags::RequiredArgument | ZyppFlags::Repeatable | ZyppFlags::Deprecated | ZyppFlags::Hidden,
                  ZyppFlags::StringVectorType( &InitRepoSettings::instanceNoConst()._repoFilter, ARG_REPOSITORY ),
                  ""
            }
        }}
    );
  }

  if ( _compatMode.testFlag( CompatModeBits::EnableNewOpt ) ) {
    myOpts.push_back( {{
           { "repo", 'r',
                 ZyppFlags::RequiredArgument | ZyppFlags::Repeatable,
                 ZyppFlags::StringVectorType( &InitRepoSettings::instanceNoConst()._repoFilter, ARG_REPOSITORY ),
                 // translators: -r, --repo <ALIAS|#|URI>
                 _("Work only with the specified repository.")
           }
        }}
    );
  }

  return myOpts;
}

void InitReposOptionSet::reset()
{
  InitRepoSettings::reset();
}


namespace
{

  inline std::string overrideWarning ()
  {
    return _("Option '--%s' overrides a previously set download mode.");
  }

  //A flag type that takes a download mode string and maps it to the correct Download  mode
  ZyppFlags::Value DownloadModeArgType( DownloadOptionSet &target, DownloadMode defValue ) {
    return ZyppFlags::Value (
      [defValue]() ->  boost::optional<std::string>{
        std::stringstream str;
        str << defValue;
        return str.str();
      },
      [ &target ]( const ZyppFlags::CommandOption &opt, const boost::optional<std::string> &in ){
        if (!in)
          ZYPP_THROW(ZyppFlags::MissingArgumentException(opt.name));

        if ( target.wasSetBefore() ) {
          Zypper::instance().out().warning(
            str::form( overrideWarning().c_str(), opt.name ) );
        }

        if (*in == "only")
          target.setMode( DownloadOnly );
        else if (*in == "in-advance")
          target.setMode( DownloadInAdvance );
        else if (*in == "in-heaps")
          target.setMode( DownloadInHeaps );
        else if (*in == "as-needed")
          target.setMode( DownloadAsNeeded );
        else {
          ZYPP_THROW( ZyppFlags::InvalidValueException( opt.name, *in, str::form(_("Available download modes: %s"), "only, in-advance, in-heaps, as-needed") ) );
        }
        return;
      }
    );
  }

  //A flag type that directly writes the DownloadMode variable
  ZyppFlags::Value DownloadModeNoArgType( DownloadOptionSet &target, DownloadMode setFlag ) {
    return ZyppFlags::Value (
      ZyppFlags::noDefaultValue,
      [ &target, setFlag ]( const ZyppFlags::CommandOption &opt, const boost::optional<std::string> & ){

        if ( target.wasSetBefore() ) {
          Zypper::instance().out().warning(
            str::form( overrideWarning().c_str(), opt.name ) );
        }

        target.setMode( setFlag );
        return;
      }
    );
  }
}

zypp::DownloadMode DownloadOptionSet::mode() const
{
  return _mode;
}

void DownloadOptionSet::setMode(const zypp::DownloadMode &mode)
{
  _mode = mode;
  _wasSetBefore = true;
}

bool DownloadOptionSet::wasSetBefore() const
{
  return _wasSetBefore;
}

std::vector<ZyppFlags::CommandGroup> DownloadOptionSet::options()
{
  // All the flags are defined as Repeatable, even though they do not fill a list, we want it to be possible to override
  // the download mode. This is more in sync with the previous behaviour
  return {{{
        { "download", '\0', ZyppFlags::RequiredArgument | ZyppFlags::Repeatable, DownloadModeArgType( *this, _mode ),
              // translators: --download
              str::Format(_("Set the download-install mode. Available modes: %s") ) % "only, in-advance, in-heaps, as-needed"
        },
        { "download-only", 'd', ZyppFlags::NoArgument | ZyppFlags::Repeatable, DownloadModeNoArgType( *this, DownloadMode::DownloadOnly ),
              // translators: -d, --download-only
              _("Only download the packages, do not install.")
        },
        { "download-in-advance", '\0', ZyppFlags::NoArgument | ZyppFlags::Repeatable | ZyppFlags::Hidden, DownloadModeNoArgType( *this, DownloadMode::DownloadInAdvance ), "" },
        { "download-in-heaps",   '\0', ZyppFlags::NoArgument | ZyppFlags::Repeatable | ZyppFlags::Hidden, DownloadModeNoArgType( *this, DownloadMode::DownloadInHeaps ), "" },
        { "download-as-needed",  '\0', ZyppFlags::NoArgument | ZyppFlags::Repeatable | ZyppFlags::Hidden, DownloadModeNoArgType( *this, DownloadMode::DownloadAsNeeded ), "" }
  }}};
}

void DownloadOptionSet::reset()
{
  _mode = ZConfig::instance().commit_downloadMode();
  _wasSetBefore = false;
}


std::vector<ZyppFlags::CommandGroup> NotInstalledOnlyOptionSet::options()
{
  return {{{
        { "installed-only",      'i',  ZyppFlags::NoArgument, ZyppFlags::WriteFixedValueType( _mode, Mode::ShowOnlyInstalled ),
              // translators: -i, --installed-only
              _("Show only installed packages.")
        },
        { "not-installed-only",  'u',  ZyppFlags::NoArgument, ZyppFlags::WriteFixedValueType( _mode, Mode::ShowOnlyNotInstalled ),
              // translators: -u, --not-installed-only
              _("Show only packages which are not installed.")
        },
        // bsc#972997: Prefer --not-installed-only over misleading --uninstalled-only
        { "uninstalled-only",    '\0', ZyppFlags::NoArgument | ZyppFlags::Hidden | ZyppFlags::Deprecated, ZyppFlags::WriteFixedValueType( _mode, Mode::ShowOnlyNotInstalled ), "" }
  }}};
}

void NotInstalledOnlyOptionSet::reset()
{
  _mode == Mode::Default;
}
