# Copyright (c) 2003, Mikhael Goikhman
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

package FVWM::Module::Tk;

use 5.004;
use strict;

use FVWM::Module::Toolkit qw(base Tk Tk::Dialog Tk::ROText);

sub new ($@) {
	my $class = shift;
	# support the old API with the first top-level argument
	my $top = shift if @_ && UNIVERSAL::isa($_[0], "Tk::Toplevel");
	my %params = @_;

	$top = delete $params{TopWindow} if exists $params{TopWindow};
	my $self = $class->SUPER::new(%params);

	$self->internalDie("TopWindow given in constructor is not Tk::Toplevel")
		unless $top || UNIVERSAL::isa($top, "Tk::Toplevel");
	unless ($top) {
		$top = MainWindow->new;
		$top->withdraw;
	}

	$self->{topWindow} = $top;

	return $self;
}

sub eventLoop ($) {
	my $self = shift;
	my @params = @_;

	$self->eventLoopPrepared(@params);
	my $top = $self->{topWindow};
	$top->fileevent($self->{istream},
		readable => sub {
			unless ($self->processPacket($self->readPacket)) {
				if ($self->{disconnected}) {
					# Seems like something does not want to exit - force it.
					# For example, a new Tk window is launched on ON_EXIT.
					$top->destroy if defined $top && defined $top->{Configure};
					$self->debug("Forced to exit to escape eventLoop, fix the module", 0);
					exit 1;
				}
				$self->eventLoopFinished(@params);
				$top->destroy;
			} else {
				$self->eventLoopPrepared(@params);
			}
		}
	);
	MainLoop;
}

sub showError ($$;$) {
	my $self = shift;
	my $error = shift;
	my $title = shift || ($self->name . " Error");

	my $top = $self->{topWindow};

	my $dialog = $top->Dialog(
		-title => $title,
		-bitmap => 'error',
		-default_button => 'Close',
		-text => $error,
		-buttons => ['Close', 'Close All Errors', 'Exit Module'],
	);
	my $btn = $dialog->Show;

	$self->terminate if $btn eq 'Exit Module';
	$self->send("All ('$title') Close") if $btn eq 'Close All Errors';
}

sub showMessage ($$;$) {
	my $self = shift;
	my $msg = shift;
	my $title = shift || ($self->name . " Message");

	$self->{topWindow}->messageBox(
		-icon => 'info',
		-type => 'ok',
		-title => $title,
		-message => $msg,
	);
}

sub showDebug ($$;$) {
	my $self = shift;
	my $msg = shift;
	my $title = shift || ($self->name . " Debug");

	my $dialog = $self->{tkDebugDialog};

	my $top = $self->{topWindow};
	unless (defined $top && defined $top->{Configure}) {
		# in the constructor (too early) or in destructor (too late)
		$self->FVWM::Module::Toolkit::showDebug($msg, $title);
		return;
	}

	if (!$dialog) {
		# Tk's Dialog widgets are too damn inflexible.
		# It's less hassle to build one from scratch.
		$dialog = $top->Toplevel(-title => $title);
		my $scroll = $dialog->Frame()->pack(-expand => 1, -fill => 'both');
		my $bottom = $dialog->Frame()->pack(-expand => 0, -fill => 'x');
		my $text = $scroll->Scrolled('ROText',
			-bg => 'white',
			-wrap => 'word',
			-scrollbars => 'oe',
		)->pack(-expand => 1, -fill => 'both');

		$dialog->protocol('WM_DELETE_WINDOW', sub { $dialog->withdraw(); });
		my @packOpts = (-side => 'left', -expand => 1, -fill => 'both');

		$bottom->Button(
			-text => 'Close',
			-command => sub { $dialog->withdraw(); },
		)->pack(@packOpts);
		$bottom->Button(
			-text => 'Clear',
			-command => sub { $text->delete('0.0', 'end'); },
		)->pack(@packOpts);
		$bottom->Button(
			-text => 'Save',
			-command => sub {
				my $file = $dialog->getSaveFile(-title => "Save $title");
				return unless defined $file;
				if (!open(OUT, ">$file")) {
					$self->showError("Couldn't save $file: $!", 'Save Error');
					return;
				}
				print OUT $text->get('0.0', 'end');
				close(OUT);
			},
		)->pack(@packOpts);

		$self->{tkDebugDialog} = $dialog;
		$self->{tkDebugTextWg} = $text;
	} else {
		$dialog->deiconify() if $dialog->state() eq 'withdrawn';
	}
	my $text = $self->{tkDebugTextWg};
	$text->insert('end', "$msg\n");
	$text->see('end');
}

sub topWindow ($) {
	return shift->{topWindow};
}

1;

__END__

=head1 NAME

FVWM::Module::Tk - FVWM::Module with the Tk widget library attached

=head1 SYNOPSIS

Name this module TestModuleTk, make it executable and place in ModulePath:

    #!/usr/bin/perl -w

    use lib `fvwm-perllib dir`;
    use FVWM::Module::Tk;
    use Tk;  # preferably in this order

    my $top = new MainWindow(-name => "Simple Test");
    my $id = $top->wrapper->[0];

    my $module = new FVWM::Module::Tk(
        TopWindow => $top,
        Mask => M_ICONIFY | M_ERROR,  # Mask may be omitted
        Debug => 2,
    );
    $top->Button(
        -text => "Close", -command => sub { $top->destroy; }
    )->pack;

    $module->addDefaultErrorHandler;
    $module->addHandler(M_ICONIFY, sub {
        my $id0 = $_[1]->_win_id;
        $module->send("Iconify off", $id) if $id0 == $id;
    });
    $module->track('Scheduler')->schedule(60, sub {
        $module->showMessage("You run this module for 1 minute")
    });

    $module->send('Style "*imple Test" Sticky');
    $module->eventLoop;

=head1 DESCRIPTION

The B<FVWM::Module::Tk> class is a sub-class of B<FVWM::Module::Toolkit>
that overloads the methods B<new>, B<eventLoop>, B<showMessage>,
B<showDebug> and B<showError> to manage Tk objects as well. It also adds new
method B<topWindow>.

This manual page details only those differences. For details on the
API itself, see L<FVWM::Module>.

=head1 METHODS

Only overloaded or new methods are covered here:

=over 8

=item B<new> I<param-hash>

$module = new B<FVWM::Module::Tk> I<TopWindow> => $top, %params

Create and return an object of the B<FVWM::Module::Tk> class.
This B<new> method is identical to the (grand-)parent class method, with the
exception that a Tk top-level of some sort (MainWindow, TopLevel, Frame,
etc.) may be passed in the hash of options using the I<TopWindow> named value.
Other options in I<param-hash> are the same as described in L<FVWM::Module>.

If no top-level window is specified in the constructor, such dummy window
is created and immediatelly withdrawn. This top-level window is needed to
create Tk dialogs.

=item B<eventLoop>

From outward appearances, this methods operates just as the parent
B<eventLoop> does. It is worth mentioning, however, that this version
enters into the Tk B<MainLoop> subroutine, ostensibly not to return.

=item B<showError> I<msg> [I<title>]

This method creates a dialog box using the Tk widgets. The dialog has
three buttons labeled "Close", "Close All Errors" and "Exit Module".
Selecting the "Close" button closes the dialog. "Close All Errors" closes
all error dialogs that may be open on the screen at that time.
"Exit Module" terminates your entire module.

Good for diagnostics of a Tk based module.

=item B<showMessage> I<msg> [I<title>]

Creates a message window with one "Ok" button.

Useful for notices by a Tk based module.

=item B<showDebug> I<msg> [I<title>]

Creates a debug window with 3 buttons "Close", "Clear" and "Save".
All debug messages are added to the debug window.

"Close" withdraws the window until the next debug message arrives.

"Clear" erases the current contents of the debug window.

"Save" dumps the current contents of the debug window to the selected file.

Useful for debugging a Tk based module.

=item B<topWindow>

Returns the Tk toplevel that this object was created with.

=back

=head1 BUGS

Would not surprise me in the least.

=head1 AUTHOR

Mikhael Goikhman <migo@homemail.com>.

=head1 THANKS TO

Randy J. Ray <randy@byz.org>, author of the old classes
B<X11::Fvwm> and B<X11::Fvwm::Tk>.

Scott Smedley <ss@aao.gov.au>.

Nick Ing-Simmons <Nick.Ing-Simmons@tiuk.ti.com> for Tk Perl extension.

=head1 SEE ALSO

For more information, see L<fvwm>, L<FVWM::Module> and L<Tk>.

=cut
