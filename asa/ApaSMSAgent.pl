#!/usr/bin/perl -w

# TODO: fix ideambox targets: all or only idea :/
# jabber:x:data+message (nice!) options for plugins and for agent
# optimizations/cleanups for faster and saner, strict code 

use constant VERSION		=> "0.1.7";
use constant CONFIG_FILE	=> "/etc/jabber/sms.xml";
use constant DEVEL_RUN		=> 0; # useful when testing, things are seen as 'away' in DEVEL_RUN mode

use XML::Stream; # for config files, etc
use Net::Jabber qw(Component);
use Data::Dumper; # for dumping hashes nicely
use Tie::Cache;
use MLDBM::Sync;
use MLDBM qw(DB_File Storable);
use MLDBM qw(MLDBM::Sync::SDBM_File);
use Fcntl qw(:DEFAULT); # all those for nice and fairly quick and fail-safe mldbm storage (carefully picked!)
use Time::HiRes qw(gettimeofday); # benchmarking ;)
use strict;

# set up Stop to be run if someone kills us
$SIG{KILL} = \&Stop;
$SIG{TERM} = \&Stop;
$SIG{INT} = \&Stop;
$SIG{QUIT} = \&Stop;

use vars qw( %plugins %events %config $Connection %numbers %prio_prefs %plugin_data );
	# plugins: just $plugin{name}=1
	# events: pre_connect, post_connect, process, pre_disconnect, post_disconnect, iq, message, presence
	# then ->{to} is a regexp to match from= tag from xml stanza
	# config: loaded from xml
	# numbers: $numbers{$users_jid}->{$number}=1, $number can be 'agent' also as a special case
	# ^^^^^^^ hash tied to MLDBM !
	# prio_prefs: $prio_prefs{$users_jid}->{$plugin}=X
	# ^^^^^^^^^^ hash tied to MLDBM !
	# plugin_data: plugin information data and function handlers ;>

my %total_handle_times=(iq=>0,presence=>0,message=>0);
my %count_handle_times=(iq=>0,presence=>0,message=>0);
my %avg_handle_times=(iq=>0,presence=>0,message=>0);

print("Loading ".CONFIG_FILE."...\n");
%config=%{LoadXMLHash('config',CONFIG_FILE)};
$config{admins}->{admin} = [$config{admins}->{admin}] if ref($config{admins}->{admin}) ne 'ARRAY';

log2("Tieing numbers hash...");
my $numbers_tie=tie(%numbers,"MLDBM::Sync",$config{users_numbers_file},O_CREAT|O_RDWR,0600);
$numbers_tie->SyncCacheSize($config{numbers_tiehash_cache});

log2("Tieing prio_prefs hash...");
my $prio_prefs_tie=tie(%prio_prefs,"MLDBM::Sync",$config{users_prio_prefs_file},O_CREAT|O_RDWR,0600);
$prio_prefs_tie->SyncCacheSize($config{prio_prefs_tiehash_cache});

log2("Loading plugins...");
foreach my $plugin (@{$config{load}->{plugin}}) {
	$plugins{$plugin}=1 if require $config{plugin_dir}."/".$plugin.".pl";
};

my $STD_MSG="Add contacts like +xxxxxxxxxxx@".$config{service_name}." to your roster to send them SMS messages.\n".
	"Browse ".$config{service_name}." in your service browser to see available plugins and change their options or register if needed.\n".
	"Available plugins:";
foreach my $p (keys %plugins) {
	$STD_MSG.=" $p (v".$plugin_data{$p}->{version}.")";
};
$STD_MSG.="\nWrite plugin+ or plugin- to change priorities, for example MiastoPlusa+.\n";

my %STD_STATUS=(DEVEL_RUN ? 
	(show=>'away',status=>'This is a devel run, expect errors and weird behavior;)') :
	(show=>'online',status=>'Hi! Message/browse me for more information.') );

TriggerEvent('pre_connect');

# set up @service triggers:
RegisterEvent('iq',$config{service_name},\&ServiceInIQ); # all
RegisterEvent('presence','.+',\&ServiceInPresence); # any
RegisterEvent('message','.+',\&ServiceInMessage);
#RegisterEvent('pre_disconnect','.+',\&ServicePreDisconnect);
#RegisterEvent('post_connect','.+',\&ServicePostConnect);

log2("Initializing component...");
$Connection = new Net::Jabber::Component(debuglevel=>0, debugfile=>"stdout");
log2("Initializing callbacks...");
if (Net::Jabber->VERSION > 1.3) {
	$Connection->SetCallBacks(onauth=>\&onAuth,message=>\&InMessage,presence=>\&InPresence,iq=>\&InIQ);
} else {
	$Connection->SetCallBacks(onconnect=>\&onAuth,message=>\&InMessage,presence=>\&InPresence,iq=>\&InIQ);
};
log2("Executing component...");
my $status = $Connection->Execute(hostname=>$config{server},port=>$config{port},
				  secret=>$config{password},componentname=>$config{service_name});
if (!(defined($status))) {
	log1("ERROR: Jabber server is down or connection was not allowed. ($!)");
	exit 1;
};

# general subs:

sub SendPluginPresencesToUser {
	my ($plugin,$type,$to,$which)=@_; # which==undef -> all, if ==$plugin -> pluginonly if +48... number only
	my $base_to=$to; $base_to=~s/\/.+$//g if ($base_to =~ /\//);
	# present us
	my $pres=new Net::Jabber::Presence;
	$pres->SetTo($to);
	if ($type ne 'available') {
		$pres->SetType($type);
	} elsif (exists($pres->{TREE}->{ATTRIBS}->{type})) {
		delete $pres->{TREE}->{ATTRIBS}->{type}; # no 'available' thingy
	};
	$pres->SetShow($STD_STATUS{show}) if ($type ne 'unavailable');
	$pres->SetPriority($prio_prefs{$base_to}->{$plugin});
	if ((!$which)||(($which)&&($which eq $plugin))) {
		if (&{$plugin_data{$plugin}->{will_take_h}}($base_to)) {
			$pres->SetFrom($config{service_name}."/".$plugin);
			$Connection->Send($pres);
		};
	};
	# present the numbers
	if (!$which) { # all
		for my $k (keys %{$numbers{$base_to}}) {
			next if ($k eq 'agent');
			if (&{$plugin_data{$plugin}->{will_take_h}}($base_to,$k)) {
				$pres->SetFrom($k.'@'.$config{service_name}."/".$plugin);
				$Connection->Send($pres);
			};
		};
	} elsif (exists $numbers{$base_to}->{$which}) { # only $which number
		if (&{$plugin_data{$plugin}->{will_take_h}}($base_to,$which)) {
			$pres->SetFrom($which.'@'.$config{service_name}."/".$plugin);
			$Connection->Send($pres);
		};
	};
};

sub PushAgentToUsersRoster {
	my $who=shift;
	$who=~s/\/.+$//g if ($who=~/\//);
	return if exists($numbers{$who}->{'agent'});
	my $pres=new Net::Jabber::Presence;
	$pres->SetFrom($config{service_name});
	$pres->SetTo($who);
	$pres->SetType('subscribe');
	$Connection->Send($pres);
};

sub BroadcastPresences {
	# hmm... for all users ( for all numbers (send unavail), send unavail from service ) -?
	foreach my $user (keys %numbers) {
		foreach my $pl (keys %plugins) {
			SendPluginPresencesToUser($pl,$_[0],$user);
		};
	};
#[blank]	my $p=new Net::Jabber::Presence;
#[blank]	$p->SetFrom($config{service_name});
#[blank]	if ($_[0] eq 'available') {
#[blank]		$p->SetType($_[0]);
#[blank]		delete $p->{TREE}->{ATTRIBS}->{type};
#[blank]	} else {
#[blank]		$p->SetType($_[0]);
#[blank]	};
#[blank]	$p->SetShow($STD_STATUS{show});
#[blank]	$p->SetStatus($STD_STATUS{status});
#[blank]	foreach my $user (keys %numbers) {
#[blank]		$p->SetTo($user); $Connection->Send($p);
#[blank]	};
};

sub Stop {
	log2("Exiting...\n");
	TriggerEvent('pre_disconnect');
	log2("Sleep for 1 sec...\n");
	sleep 1;
	$Connection->Disconnect();
	TriggerEvent('post_disconnect');
#	untie %numbers; no need, it's synced and if I untie it complains :/ strange
#	untie %prio_prefs;
	exit(1);
}

sub SaveXMLHash {
        my $h=shift;
        my $tag=shift;
        my $fn=shift;
	my $logprefix=shift;
	$logprefix=($logprefix?$logprefix.": ":"");
        log3($logprefix."Saving $tag hash into $fn");
        open F,">".$fn;
        print F XML::Stream::Config2XML($tag,$h),"\n";
        close F;
};

sub LoadXMLHash {
        my $tag=shift;
        my $fn=shift;
	my $logprefix=shift;
	$logprefix=($logprefix?$logprefix.": ":"");
        log3($logprefix."Loading $tag hash from $fn") if (defined(%config));
	unless (-f $fn) {
		open F,">".$fn; print F "<".$tag."/>\n"; close F; chmod 0600,$fn;
		log3($logprefix."Created initial empty file/hash.") if (defined(%config));
	};
	return XML::Stream::XML2Config(new XML::Stream::Parser(style=>"node")->parsefile($fn));
};

sub CheckPrefsFor {
	my $who=shift;
	foreach my $k (keys %plugins) {
		unless (exists($prio_prefs{$who}->{$k})) {
			my $a=$prio_prefs{$who};
			$a->{$k}=$plugin_data{$k}->{default_prio};
			$prio_prefs{$who}=$a;
		};
	};
};

# service handlers:

sub ServiceInIQ {
	my $iq_o=shift;
	my $xmlns=shift;
	my $query_o=$iq_o->GetQuery();
	my $type=$iq_o->GetType();
	my $to=$iq_o->GetTo(); my $from=$iq_o->GetFrom();
	my $iq=new Net::Jabber::IQ;
	$iq->SetTo($from); $iq->SetFrom($to); $iq->SetID($iq_o->GetID);
	my $query=$iq->NewQuery($xmlns);
	if ($xmlns eq 'http://jabber.org/protocol/disco#info') {
		if ($type eq 'get') { 
			$iq->SetType('result');
			if ($to eq $config{service_name}) { # main
				$query->AddIdentity(category=>"gateway",type=>"sms",name=>"Bramka SMS");
				$query->AddFeature(var=>'vcard-temp');
				$query->AddFeature(var=>'jabber:iq:version');
				$query->AddFeature(var=>'jabber:iq:gateway');
				$query->AddFeature(var=>'jabber:iq:agents');
				$query->AddFeature(var=>'vcard-temp');
				$query->AddFeature(var=>'http://jabber.org/protocol/disco#info');
				$query->AddFeature(var=>'http://jabber.org/protocol/disco#items');
			} elsif (my $p=(grep {$to eq $config{service_name}."/".$_ } keys %plugins)[0]) { # plugin - bare, w/o @
				$query->AddIdentity(category=>"gateway",type=>"sms",name=>$p);
				$query->AddFeature(var=>'vcard-temp');
				$query->AddFeature(var=>'jabber:iq:version');
				$query->AddFeature(var=>'http://jabber.org/protocol/disco#info');
				$query->AddFeature(var=>'jabber:iq:register') if ($plugin_data{$p}->{needs_reg});
			}; 
			$Connection->Send($iq);
		};
	} elsif ($xmlns eq 'http://jabber.org/protocol/disco#items') {
		if ($type eq 'get') {
			$iq->SetType('result');
			if ($to eq $config{service_name}) {
				for my $k (sort { $a cmp $b } keys %plugins) {
					$query->AddItem(name=>$k,jid=>$config{service_name}."/".$k);
				};
			}; # else empty
			$Connection->Send($iq);
		};
	} elsif ($xmlns eq 'jabber:iq:agents') { # for old clients
		if ($type eq 'get') { 
			$iq->SetType('result');
			if ($to eq $config{service_name}) { # main
				for my $pl (keys %plugins) {
					my $qa=$query->AddAgent('jabber:iq:agent');
					$qa->SetJID($config{service_name}."/".$pl);
					$qa->SetName($pl);
					$qa->SetService('sms');
					$qa->SetRegister('') if ($plugin_data{$pl}->{needs_reg});
				};
			}; 
			$Connection->Send($iq);
		};
	} elsif ($xmlns eq 'vcard-temp') { # let it be global vcard for anything queried
		if ($type eq 'get') {
			my %vc=(
				FN=>"Bramka SMS",
				DESC=>"Plugin based SMS Agent",
			);
			AddVCardToIQ($iq,shift,\%vc);
			$iq->SetType('result');
			$Connection->Send($iq);
		};
	} elsif ($xmlns eq 'jabber:iq:version') {
		if ($type eq 'get') {
			$iq->SetType('result');
			if ($to eq $config{service_name}) { # main
				$query->SetName("Apatsch's SMS Agent");
				$query->SetVer(VERSION);
				$query->SetOS($^O);
			} elsif (my $p=(grep {$to =~ /$config{service_name}\/$_$/ } keys %plugins)[0]) { # plugin or it's contact
				$query->SetName("Apatsch's SMS Agent - ".$p);
				$query->SetVer(VERSION."/".$plugin_data{$p}->{version});
				$query->SetOS($^O);
			}; 
			$Connection->Send($iq);
		};
	} elsif (($xmlns eq 'jabber:iq:gateway')&&($to eq $config{service_name})) { # only for main, not for plugins
		if ($type eq 'get') {
			$iq->SetType('result');
			$query->SetDesc("Provide cell phone number.");
			$query->SetPrompt("Cell #");
			$Connection->Send($iq);
		} elsif ($type eq 'set') {
			my $nr=$query_o->GetPrompt();
			# FIXME needs work:
			if ($nr=~/[0-9]{11,}/) {
				$nr=~s/^.*?([0-9]{11,}).*?$/$1/;
			} elsif ($nr=~/[0-9]{9,9}/) {
				$nr=~s/^.*([0-9]{9,9}).*$/48$1/;
			} else {
				$iq->SetType('error'); $iq->SetErrorCode(406); 
				$iq->SetError('Not Acceptable'); $Connection->Send($iq);
			};
			if ($iq->GetType() ne 'error') {
				$iq->SetType('result');
				$query->SetPrompt("+$nr@".$config{service_name});
				$query->SetJID("+$nr@".$config{service_name});
				$Connection->Send($iq);
			};
		};
	};
};

sub ServiceInMessage {
	my $message_o=shift;
	my $type=shift;
	my $to=$message_o->GetTo(); my $from=$message_o->GetFrom();
	my $message=new Net::Jabber::Message;
	$message->SetTo($from); $message->SetFrom($to); $message->SetType($type);
	my $SN=$config{service_name};
	my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
	CheckPrefsFor($base_from);
	my $nr; if ($to =~ /@/) { $nr=$to; $nr=~s/@.+$//g; } else { $nr='' };
	my $body=$message_o->GetBody();
	return unless ($body ne ''); # for msgevents
	if ($nr eq '') {
		if ($to =~ /\//) { # "control message" to plugin?
			my $pl=$to; $pl=~s/^.+\///g;
			1; # TODO
		} else {
			foreach my $k (keys %plugins) {
				if ($body =~ m/$k[\+\-]/) {
					# prio change :)
					if (($body =~ m/\-$/)&&($prio_prefs{$base_from}->{$k}>0)) {
						my $a=$prio_prefs{$base_from}; $a->{$k}--; $prio_prefs{$base_from}=$a;
					} elsif ($prio_prefs{$base_from}->{$k}<20) {
						my $a=$prio_prefs{$base_from}; $a->{$k}++; $prio_prefs{$base_from}=$a;
					};
					SendPluginPresencesToUser($k,'available',$from);
				};		
			};
			my $b=$STD_MSG."\n--- Your numbers I'm aware of:\n";
			foreach my $n (keys %{$numbers{$base_from}}) { $b.=$n." " unless ($n eq 'agent'); };
			$b.="\n--- Your plugin priority preferences:\n";
			foreach my $n (keys %{$prio_prefs{$base_from}}) { $b.=$n." = ".$prio_prefs{$base_from}->{$n}."\n"; };
			$message->SetBody($b);
			$Connection->Send($message);
		};
		# admin commands (work everywhere, for the agent and plugins...):
		if (grep { $_ eq $base_from } @{$config{admins}->{admin}}) { # from admin
			if ($body =~ /^\/(wall|motd) /i) { # wall cmd
				log3("wall from admin $base_from");
				$body=~s/^..... //g; # ;)
				$message->SetBody($body);
				$message->SetType('normal');
				$message->SetSubject("MOTD message from admin of ".$config{service_name});
				my $ackbody=$body." - has been sent to:";
				foreach my $user (keys %numbers) {
					$message->SetTo($user); $Connection->Send($message);
					$ackbody=$ackbody." ".$user;
				};
				$message->SetType($type); $message->SetTo($from); $message->SetBody($ackbody);
				$Connection->Send($message);
			} elsif ($body =~ /^\/dumpplugins$/i) {
				log3("dumpplugins from admin $base_from");
				$message->SetBody(Dumper(\%plugins));
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
			} elsif ($body =~ /^\/dumpconfig$/i) {
				log3("dumpconfig from admin $base_from");
				$message->SetBody(Dumper(\%config));
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
#			} elsif ($body =~ /^\/dumpnumbers$/i) {
#				log3("dumpnumbers from admin $base_from");
#				$message->SetBody(Dumper(\%numbers));
#				$message->SetType($message_o->GetType());
#				$Connection->Send($message);
#			} elsif ($body =~ /^\/dumpprio_prefs$/i) {
#				log3("dumpprio_prefs from admin $base_from");
#				$message->SetBody(Dumper(\%prio_prefs));
#				$message->SetType($message_o->GetType());
#				$Connection->Send($message);
			} elsif ($body =~ /^\/dumpplugin_data$/i) {
				log3("dumpplugin_data from admin $base_from");
				$message->SetBody(Dumper(\%plugin_data));
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
			} elsif ($body =~ /^\/dumpusers$/i) {
				log3("dumpusers from admin $base_from");
				my $b="Users:\n";
				foreach my $user (keys %numbers) {
					$b.=$user." ";
				};
				$message->SetBody($b);
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
			} elsif ($body =~ /^\/stats$/i) {
				log3("stats from admin $base_from");
				my $b="Users #".(keys %numbers);
                my $numbers_count = 0;
				foreach my $user (keys %numbers) {
                    $numbers_count += keys %{$numbers{$user}};
                };
                $b .= ", numbers #".$numbers_count;
				$message->SetBody($b);
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
			} elsif ($body =~ /^\/help$/i) {
				log3("help from admin $base_from");
				$message->SetBody("/wall or /motd - wall message\n".
					"/stats - print basic statistic info\n".
					"/DUMPPLUGINS - dump plugins hash\n".
					"/DUMPCONFIG - dump config hash\n".
#					"/dumpnumbers - dump numbers hash\n".
#					"/dumpprio_prefs - dump prio_prefs hash\n".
					"/DUMPPLUGIN_DATA - dump plugin_data hash\n".
					"/DUMPUSERS - dump users\n");
				$message->SetType($message_o->GetType());
				$Connection->Send($message);
			};
		};
	} else { # to number, call appropriate plugin InMessage handler
		if ($message_o->GetTo('jid')->GetResource ne '') {
			my $plug=$message_o->GetTo('jid')->GetResource;
			if ((exists $plugins{$plug})&&(&{$plugin_data{$plug}->{will_take_h}}($base_from,$nr))) {
				&{$plugin_data{$plug}->{inmessage_h}}($message_o,$type);
			};
		} else {
			# 1. order plugins by users prio_pref
			my @plugins=keys %plugins;
			@plugins=sort { $prio_prefs{$base_from}->{$a} <=> $prio_prefs{$base_from}->{$b} } @plugins;
			# 2. now go from the highest (end) to the lowest (begin) plugin to see if will accept (reg, nr, etc)
			while (@plugins) {
				my $plug=pop @plugins;
				my $h=$plugin_data{$plug}->{will_take_h};
				if ( &{$h}($base_from,$nr) ) {
					# good, call it's inmessage
					my $inmh=$plugin_data{$plug}->{inmessage_h};
					$message_o->SetTo($nr."@".$config{service_name}."/".$plug);
					&{$inmh}($message_o,$type); # hand it to plugin ;)
					last;
				};	
			};
		};
	};
};

sub ServiceInPresence {
	my $presence_o=shift;
	my $type=shift;
	my $to=$presence_o->GetTo(); my $from=$presence_o->GetFrom();
	my $presence=new Net::Jabber::Presence;
	$presence->SetTo($from); $presence->SetFrom($to);
	my $SN=$config{service_name};
	my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
	CheckPrefsFor($base_from);
	if ($to eq $SN) { # service + plugins
		if ($type eq 'subscribe') {
			$presence->SetType('subscribed');
			$Connection->Send($presence);

			delete $presence->{TREE}->{ATTRIBS}->{type}; # XMPP
			$presence->SetShow($STD_STATUS{show});
			$presence->SetStatus($STD_STATUS{status});
			$Connection->Send($presence);

			{ my $a=$numbers{$base_from}; $a->{'agent'}=1; $numbers{$base_from}=$a; }; # tied hash needs this way
			foreach my $plugin (keys %plugins) {
				SendPluginPresencesToUser($plugin,'available',$from,$plugin);
			};
		} elsif ($type eq 'unsubscribe') {
			$presence->SetType('unsubscribed');
			$Connection->Send($presence);

			if ($numbers{$base_from}->{'agent'}==1) {
				my $a=$numbers{$base_from}; delete $a->{'agent'}; $numbers{$base_from}=$a; # tied, needs this
			};
		} elsif ($type eq 'probe') {
			foreach my $plugin (keys %plugins) {
				SendPluginPresencesToUser($plugin,'available',$from,$plugin);
			};
		};
	} elsif ($to =~ /@/) { # someting with @ :P
		if ($to =~ /^\+(48[0-9]{9}|[0-9]{11,})\@/) { # generic nr, good nr 
			my $nr=$to; $nr=~s/@.+$//;
			if ($type eq 'subscribe') {
				$presence->SetType('subscribe'); # ask him for auth, we need to maintain his list of numbers
				$Connection->Send($presence);

				$presence->SetType('subscribed'); # ?
				$Connection->Send($presence);

				PushAgentToUsersRoster($from);
			} elsif ($type eq 'unsubscribe') {
				$presence->SetType('unsubscribed');
				$Connection->Send($presence);

				if ($numbers{$base_from}->{$nr}==1) {
					my $a=$numbers{$base_from}; delete $a->{$nr}; $numbers{$base_from}=$a; # tied, needs this
				};
			} elsif ($type eq 'subscribed' or (!defined($type))) {
				# will send to us when will remove the number,that's why it was needed
				{ my $a=$numbers{$base_from}; $a->{$nr}=1; $numbers{$base_from}=$a; }; # tied hash 
				# handle plugins action:
				foreach my $plugin (keys %plugins) {
					SendPluginPresencesToUser($plugin,'available',$from,$nr);
				};
			} elsif (($type eq 'available')||($type eq 'probe')) {
				foreach my $plugin (keys %plugins) {
					SendPluginPresencesToUser($plugin,'available',$from,$nr);
				};
			};
		} else { # bad contact
			if ($type eq 'subscribe') {
				$presence->SetType('subscribed'); # ok let him see it, but
				$Connection->Send($presence);

				delete $presence->{TREE}->{ATTRIBS}->{type};
				$Connection->Send($presence);

				$presence->SetType('unsubscribed'); # cancel it - he'll see this mess and see it's wrong 
				$Connection->Send($presence);

				$presence->SetType('unavailable'); # cancel it 
				$Connection->Send($presence);

				PushAgentToUsersRoster($from);
				# and send message bad contact
				my $m=new Net::Jabber::Message;
				$m->SetFrom($config{service_name}); $m->SetTo($presence->GetTo());
				$m->SetType(''); $m->SetBody($STD_MSG."\n---> You added contact in bad format.");
				$Connection->Send($m);
			};
		};
	};
};

sub ServicePreDisconnect {
	BroadcastPresences('unavailable');
};

sub ServicePostConnect {
	BroadcastPresences('available');
};

# === general event handling subs ===

sub onAuth {
	log2("Connected to ".$config{server}.":".$config{port}.".");
	TriggerEvent('post_connect');
	# ========== MAIN LOOP: (now in Execute) 
#	log2("Processing.");
#	while (1) {
#		Stop() unless defined($Connection->Process(1));
#		#TriggerEvent('process'); silent for now
#	}
#	log1("ERROR: The connection was killed.");
#	Stop();
};

sub InMessage {
	my $sid=shift;
	my $message=shift;
	my ($from,$to,$type)=($message->GetFrom(),$message->GetTo(),$message->GetType());
	log3("Message: from=$from to=$to type=$type");
	log4("Message: from=$from to=$to type=$type body=[".$message->GetBody()."]");
	return if ($type eq 'error'); # after log
	TriggerEvent('message',$to,$message,$type);
};

sub InIQ {
	my $sid=shift;
	my $iq=shift;
	my $query=$iq->GetQuery();
	my ($from,$to)=($iq->GetFrom(),$iq->GetTo());
	unless ($query) { # vCard? ;) - tricky. ugly. working.
		my $tree=$iq->GetTree();
		my @vc_kids=grep($_->get_tag() eq 'vCard',$tree->children());
		if (@vc_kids) {
			my $vc_node=$vc_kids[0];
			if ($vc_node->get_attrib('xmlns') eq 'vcard-temp') {
				log3("IQ: from=$from to=$to xmlns=vcard-temp");
				return if ($iq->GetType() eq 'error'); # after log
				TriggerEvent('iq',$to,$iq,'vcard-temp',$vc_node);
			};
		};
	} else {
		my $xmlns=$query->GetXMLNS();
		log3("IQ: from=$from to=$to xmlns=$xmlns");
		return if ($iq->GetType() eq 'error'); # after log
		TriggerEvent('iq',$to,$iq,$xmlns) if ( (!DEVEL_RUN)||(DEVEL_RUN && ($xmlns ne "jabber:iq:version")) );
	};
};

sub InPresence {
	my $sid=shift;
	my $presence=shift;
	# class sub calls only here, once. Quicker than many class sub calls. Trust me ;)
	my ($from,$to,$type)=($presence->GetFrom(),$presence->GetTo(),$presence->GetType());
	log3("Presence: from=$from to=$to type=$type");
	return if ($type eq 'error');
	TriggerEvent('presence',$to,$presence,$type);
};

# === event engine subs ===

sub TriggerEvent {
	my $type=shift;
	my $to=shift||'.+';
	log3("TriggerEvent: $type to $to") unless (($type eq 'iq')||($type eq 'message')||($type eq 'presence'));
	my $st_time=gettimeofday;
	return unless (exists($events{$type}));
	# propagate
	foreach my $tos (keys %{$events{$type}}) {
		if ($to =~ /$tos/) {
			foreach my $h (@{$events{$type}->{$tos}}) {
				&{$h}(@_);
			};
		};
	};
	if (($type eq 'iq')||($type eq 'message')||($type eq 'presence')) {
		my $t=gettimeofday-$st_time;
		$count_handle_times{$type}++;
		$total_handle_times{$type}+=$t;
		$avg_handle_times{$type}=$total_handle_times{$type}/$count_handle_times{$type};
		log3(sprintf("TriggerEvent: done in %.2f sec, avg iq=%.2f pres=%.2f msg=%.2f tot iq=%.2f pres=%.2f msg=%.2f",
			$t,$avg_handle_times{iq},$avg_handle_times{presence},$avg_handle_times{message},
			$total_handle_times{iq},$total_handle_times{presence},$total_handle_times{message}));
	};
};

sub RegisterEvent {
	my $type=shift;
	my $to=shift;
	my $handler = shift;
	push(@{$events{$type}->{$to}},$handler);
};

# === xml subs ===

sub AddVCardToIQ { # yes, it's ugly...
	my $iq=shift;
	my $vc_node=shift;
	my $hash=shift;
	
	my $raw_xml="<vCard xmlns=\"vcard-temp\" ";
	if (defined $vc_node) {
		$raw_xml.="version=\"".$vc_node->get_attrib('version')."\" " if ($vc_node->get_attrib('version'));
		$raw_xml.="prodid=\"".$vc_node->get_attrib('prodid')."\" " if ($vc_node->get_attrib('prodid'));
	};
	$raw_xml.=">\n";
	foreach my $k (keys %{$hash}) {
		$raw_xml.="<$k>".EscapeXML_heavy($hash->{$k})."</$k>\n";
	};
	$raw_xml.="</vCard>\n";
	$iq->InsertRawXML($raw_xml);
	return $iq;
};

sub EscapeXML_heavy # from XML::Stream
{   
	my $data = shift;
	if (defined($data)) { 
		$data =~ s/&/&amp;/g;
		$data =~ s/</&lt;/g;
		$data =~ s/>/&gt;/g;
		$data =~ s/\"/&quot;/g;
		$data =~ s/\'/&apos;/g;
	}
	return $data;
};

# === logging subs ===

sub log1 {
  return unless $config{verbose} >= 1;
  my $msg = shift;
  print STDERR "WARN: $msg\n";
}

sub log2 {
  return unless $config{verbose} >= 2;
  my $msg = shift;
  print "INFO: $msg\n";
}

sub log3 {
  return unless $config{verbose} >= 3;
  my $msg = shift;
  print "DBUG: $msg\n";
}

sub log4{
  return unless $config{verbose} >= 4;
  my $msg = shift;
  print "EXTR_DB: $msg\n";
}
