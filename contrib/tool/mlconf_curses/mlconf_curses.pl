#!/usr/bin/perl

use strict;
use Curses;
use Data::Dumper;


###########
# global variables
###########
#
my $VERSION="0.1.1";
my $TITLE = "mlterm configulator on Curses v.$VERSION";
my $TERM="mlterm" || $ENV{TERM}; ### XXX:drop support for other T.E.s ?
my $TTY = "/dev/tty"; ### XXX TBD:support remote control!

my $FG_COLOR = 'white'; ## Used when displaying menu(s).
my $BG_COLOR = 'black'; ## NOT used for terminal param.
my $EDIT_COLOR = 'red'; ## NOT used for terminal param.
my $SELECT_COLOR = 'green'; ## NOT used for terminal param.

my $MAGIC_ERROR = "error"; ## XXX

### variables below should  not be touched outside config*
my %config_tree; 
my $config_section_stat = 0; #selected section
my $config_section_width = 2;

my @config_section_list; ## section index to name conversion

my $ENTRY_NAME=0;
my $ENTRY_FUNC=1;
my $ENTRY_CTXT=2; ## setting for display etc.
my $ENTRY_DATA=3; ## value entered from user
my $ENTRY_INITIAL=4; ## initial value from terminal emulator
my $ENTRY_INDEX=5; ## id

my $SECTION_NAME=0;
my $SECTION_INDEX=1;
my $SECTION_LIST=2; ## array of entry
my $SECTION_SELECTED=3; ## id of selected entry
my $SECTION_ENTRYWIDTH=4; ### XXX not used yet


### variables below should  not be touched outside display*
my $display_state = 0;   ## 0.section / 1.entry /2.edit
my $ST_SECT=0;           ## XXX  ENUM
my $ST_ENTRY=1;
my $ST_MODIFY=2;

my %CURSES_COLOR_PAIR;
my %CURSES_COLORS = 
    ( 'black' => COLOR_BLACK,         'cyan'          => COLOR_CYAN,
      'green' => COLOR_GREEN,         'magenta'       => COLOR_MAGENTA,
      'red'   => COLOR_RED,           'white'         => COLOR_WHITE,
      'yellow'=> COLOR_YELLOW,        'blue'       => COLOR_BLUE);

### curses windows
my $window_top;
my $window_sect;
my $window_entry;
my $window_demo;

my $MARGIN_SIDE = 1;
my $MARGIN_TOP = 1;

my $SECTION_SPAN = 2;
my $ENTRY_SPAN = 2;

my $window_grab; ## FixMe may be eliminated
my @color_table = ( ## mlterm only??
		   'white',
		   'black',
		   'red',
		   'green',
		   'yellow',
		   'blue',
		   'magenta',
		   'cyan',
#                   'gray',
#		   'lightgray',
#		   'pink',
#		   'brown',
#		   'priv_fg',
#		   'priv_bg'
		  );
my @color_table_map = ( ## mlterm only??
		       'white',
		       'black',
		       'red',
		       'green',
		       'yellow',
		       'blue',
		       'magenta',
		       'cyan',
#		       'white',
#		       'white',
#		       'red',
#		       'yellow',
#		       "white", ### XXX FixMe: 
#		       "white"
		      );
########################
# Misc.
########################
sub tabpos(@){
    my $pos = shift;
    my $tab = shift || 8;
    $pos = int(($pos +1 ) / $tab  +1) * $tab;
    return $pos;
}
########################
## Signals
########################

#window resize
$SIG{WINCH}=sub {
    display_final(); ### XXX *BUGGY* ###
#resize term only is enough?
    display_init();

    display_section_all();
    display_entry_all();

    display_refresh($window_sect);
    display_refresh($window_entry);

};
#interrupt  XXX use END block ?
$SIG{INT}=sub{
    display_final();
    exit(1);
};

################################
# config
################################
sub config_init_fg(){
    return if ($TERM ne "mlterm");
    my $term_color;
    $term_color = comm_getparam_mlterm("Foreground");
    my $color;
    foreach $color (@color_table_map){
	if ($term_color eq $color){
		$FG_COLOR = $color;
	    }
    }
}


sub config_init_bg(){
    return if ($TERM ne "mlterm");
    my $term_color;
    $term_color = comm_getparam_mlterm("Background");
    my $color;
    foreach $color (@color_table_map){ ### XXX check to avoid fg_color
	if ($term_color eq $color){
	    $BG_COLOR = $color;
	}
    }
    if ($BG_COLOR eq $FG_COLOR){ ### can't see anythig
	if ($FG_COLOR eq "black"){
	    $BG_COLOR = "white";
	}else{
	    $BG_COLOR = "black";
	}
    }
}

sub config_entry_add(@){
    my $data_ref = shift;
    ###FixMe check data structure 
    my ($section, $name, $proc_get, $proc_set, $proc_disp, $context) = @$data_ref;
    my $dest;

    if ( $dest = @{$config_tree{$section}}[$SECTION_LIST]){
 	push @{$dest}
	    , [ $name , #NAME
		{       #FUNC
		 "get" => $proc_get ,
		 "set" => $proc_set ,
		 "disp" => $proc_disp} ,
		$context,#CTXT
		undef,   #DATA => filled after modify
		undef,   #INITIAL => filled by value from terminal
		config_section_size($section)   # index
	      ] ;
	$config_tree{section}[$SECTION_ENTRYWIDTH] = length($name) 
	    if ($config_tree{$section}[$SECTION_ENTRYWIDTH] < length($name));
    }
}

sub config_entry_add_from_list(@){
    my $ref_entry;
    foreach $ref_entry (@_){
	config_entry_add($ref_entry);
    }
}

sub config_section_add(@){
    my $name;
    while( $name = shift){
	unless ( $config_tree{ $name}){
	    my $index = @config_section_list;
	    $config_tree{ $name}=[ $name,  #                 #SECTION_NAME
				   $index, #serial           #       _INDEX
				   [],     #list of entires   #       _LIST
				   0,      #selected entry   #       _SELECTED
				   0       #length of entry  #       _ENTRYWIDTH
				];
	    $config_section_list[ $index] = $name;
	    $config_section_width = length( $name)
		if ( $config_section_width  < length( $name));
	}
    }
}
## return an array of entry (in the section)
sub config_entry_list(@){
    my $section_name = shift || config_section_get_cur_name(); 
    my $entry_list= ${$config_tree{$section_name}}[$SECTION_LIST];
    return @$entry_list;
}

## return the index number  of current section
sub config_section_get_cur_index(){
    return $config_section_stat;
}

## return the name of current section
sub config_section_get_cur_name(){
    return $config_section_list[$config_section_stat];
}

## select the section of index number
sub config_section_set_cur_index(@){
    $config_section_stat = shift;
}

## return index number of the selected entry (in the section)
sub config_entry_get_cur_index(@){ 
    my $section_name = shift || config_section_get_cur_name();
    return $config_tree{$section_name}[$SECTION_SELECTED];	
}
## select the entry of index number (in the section)
sub config_entry_set_cur_index(@){
    my $entry_index = shift;
    my $section_name = shift || config_section_get_cur_name();
    $config_tree{$section_name}[$SECTION_SELECTED] = $entry_index;
}
## return the largest length of section title (convenience func.)
sub config_section_get_width(@){
    return $config_section_width;
}
## return the largest length of section title (convenience func.)
sub config_entry_get_width(@){
    my $section_name = shift || config_section_get_cur_name();
    return $config_tree{$section_name}[$SECTION_ENTRYWIDTH];
}

sub config_entry_disp(@){
    my $window = shift;
    my $section_name = shift;
    my $entry_index = shift;
    my $x = shift;
    my $y = shift;

    my @entry_list = config_entry_list($section_name);
    my $entry = $entry_list[$entry_index];

    my $func_hash = $$entry[$ENTRY_FUNC];
    my $get_func = $$func_hash{"get"};
    my $display_func = $$func_hash{"disp"};
    my $entry_name = $$entry[$ENTRY_NAME];
    my $oldvalue;

    $window_grab = $window_entry;
    unless (defined $$entry[$ENTRY_INITIAL] || $$entry[$ENTRY_INITIAL] eq "error"){
	$$entry[$ENTRY_INITIAL] = &$get_func( $entry_name) ;
	$$entry[$ENTRY_DATA] = $$entry[$ENTRY_INITIAL] ;
	#$$entry[$ENTRY_INITIAL] =undef() if ($$entry[$ENTRY_INITIAL] eq "error");
    }
    my $result =
	&$display_func( $window, $entry, $x, $y);
    $$entry[$ENTRY_DATA] = $result;
    return $result;
}

sub config_entry_get_data(@){
    my $entry = shift ;
    return $$entry[$ENTRY_DATA] if defined($$entry[$ENTRY_DATA]);
    return $$entry[$ENTRY_INITIAL];
}

sub config_section_size(@){
    my $section = shift ||config_section_get_cur_name() ;
    $section = $config_tree{ $section}[$SECTION_LIST];
    return scalar(@$section);
}

sub config_apply_all(){
    my $section;
    foreach $section (keys %config_tree){
	config_apply_section($section);
    }
}
sub config_apply_section(){
## have to process per section for special combination 
# such as xim:locale
    my $section= shift || config_section_get_cur_name();
    my $entry;
    foreach $entry (@{$config_tree{$section}[2]}){
	if ( $$entry[$ENTRY_INITIAL] ne $$entry[$ENTRY_DATA]){
	    &{$$entry[$ENTRY_FUNC]{"set"}}($$entry[$ENTRY_NAME],
					   $$entry[$ENTRY_DATA]
					  );
	}
	$$entry[$ENTRY_INITIAL]=$$entry[$ENTRY_DATA] ; ### XXX check result
    }
}

sub config_revert_all(){
    my $section;
    foreach $section (keys %config_tree){
	config_revert_section($section);
    }
}
sub config_revert_section(@){
    my $section = shift || config_section_get_cur_name();
    my $entry;
    foreach $entry (@{$config_tree{$section}[2]}){
	if ( $$entry[$ENTRY_INITIAL] ne $$entry[$ENTRY_DATA]){
	    $$entry[$ENTRY_DATA]=$$entry[$ENTRY_INITIAL] ;
	}
    }
}

########################
# display
########################
sub display_main(@){
    if( $display_state == $ST_SECT){
	return display_section_select(); ## select section
    }
    if( $display_state == $ST_ENTRY){
	return display_entry_select();
    }
    if( $display_state == $ST_MODIFY){
	return display_entry_edit();
    }
}

sub display_clear(@){
    my $window = shift;
    my ($x,$y);
    $window->getmaxyx($y,$x);
    my $line;
    foreach $line (0 .. $y){
	display_str($window, " " x $x , 0, $line, $FG_COLOR, $BG_COLOR );
    }
}

sub display_final(){
    $window_sect->delwin();
    $window_entry->delwin();
    $window_top->delwin();
    %CURSES_COLOR_PAIR = {};
    endwin();
}

sub display_refresh(@){
    my $window = shift;
    $window->refresh();
}

sub display_init(){
    $window_top = new Curses || die "FATAL: curses is not ready";
    if (has_colors){
	start_color();
    }
    else{
	die "FATAL: expected to have a color terminal.";
    }
    $window_grab = $window_top;
    noecho();
    $window_top->keypad(1);
    $window_top->attrset(0); #reset all attributes
    config_init_fg(); ## getch needs a curses window.
    display_clear($window_top);
    config_init_bg(); ## getch needs a curses window.
    display_set_color( $window_top , $FG_COLOR, $BG_COLOR);
    display_clear($window_top);
    $window_top->addstr(0, 1, "$TITLE ");
    $window_top->refresh();
    $window_sect = derwin( $window_top,
			   $LINES - $MARGIN_TOP,
			   config_section_get_width() + 3 + $MARGIN_SIDE ,
			   1, 0);
    $window_sect->keypad(1);
    $window_entry = derwin( $window_top,
			    $LINES - $MARGIN_TOP - 1,
			    $COLS - config_section_get_width() - $MARGIN_SIDE - 3,
			    2 , config_section_get_width() + 4);
    $window_entry->keypad(1);
    $window_sect->refresh;
    $window_entry->refresh;
    halfdelay(5);
}

sub display_str(@){
    my $window = shift;
    my $text = shift;
    my $x = shift || 0;
    my $y = shift || 0;
    my $fgcolor = shift || $FG_COLOR;
    my $bgcolor = shift || $BG_COLOR;
    display_set_color( $window, $fgcolor, $bgcolor);
    $window->addstr( $y, $x, $text);
}

sub display_set_color(@){

    my $window = shift;
    my $fg = shift || $FG_COLOR;
    my $bg = shift || $BG_COLOR;
    ######## FixMe: how can I support pink?
    ## cached ?
    if ( $CURSES_COLOR_PAIR{"$fg:$bg"}) {
	$window->attrset(COLOR_PAIR( $CURSES_COLOR_PAIR{"$fg:$bg"}));
    } else {
	if (exists $CURSES_COLORS{$fg} && exists $CURSES_COLORS{$bg}) {
	    my @pairs = map { $CURSES_COLOR_PAIR{$_} } keys %CURSES_COLOR_PAIR;
	    my $id = 1;
	    while (grep /^$id$/, @pairs) { ++$id };
	    init_pair( $id, $CURSES_COLORS{$fg}, $CURSES_COLORS{$bg});
	    $CURSES_COLOR_PAIR{"$fg:$bg"} = $id;
	    $window->attrset(COLOR_PAIR( $CURSES_COLOR_PAIR{"$fg:$bg"}));
	}
    }
}
##########################################################
sub display_section_all(@){ 
    my $dest_window = shift || $window_sect;
    my $section;
    my $width = config_section_get_width();

    display_clear($dest_window);
    display_set_color( $dest_window, $FG_COLOR, $BG_COLOR);
    box( $dest_window, ACS_VLINE, ACS_HLINE);
    for ( $section = 0 ; $section < @config_section_list; $section ++){
	display_section( $section , $dest_window);
    }
}

sub display_section(@){
    my $section_index = shift;
    my $dest_window = shift || $window_sect;
    display_str( $dest_window, "     ",
		 $MARGIN_SIDE, 
		 $section_index * $SECTION_SPAN + $MARGIN_TOP +1 , $FG_COLOR);
    if ($section_index != config_section_get_cur_index()){ ## selected section	
	display_str( $dest_window, " $config_section_list[$section_index]",
		     $MARGIN_SIDE, 
		     $section_index * $SECTION_SPAN + $MARGIN_TOP, $FG_COLOR);
    }elsif($display_state != $ST_SECT){
	display_str( $dest_window, " $config_section_list[$section_index]",
		     $MARGIN_SIDE, $section_index * $SECTION_SPAN + $MARGIN_TOP,
		     $SELECT_COLOR);	
    }else{
	display_str( $dest_window, ">$config_section_list[$section_index]",
		     $MARGIN_SIDE, $section_index * $SECTION_SPAN + $MARGIN_TOP,
		     $EDIT_COLOR);	
    }
}

sub display_section_select(@){
    my $mode = shift;
    my $input;
    my $selected;
    while(1){
	display_section(config_section_get_cur_index());
	$input= display_getch(0, $window_sect);
	if( $input == KEY_RIGHT || $input == "\n"){ ## exit to state 1(select entry)
	    $display_state = $ST_ENTRY;
	    display_section(config_section_get_cur_index());
	    display_refresh($window_sect);
	    last;
	}elsif( $input == KEY_UP){
	    if (config_section_get_cur_index() >0){
		config_section_set_cur_index(config_section_get_cur_index() - 1);
		display_entry_all();
		display_refresh($window_entry);
	    }
	    display_section(config_section_get_cur_index() + 1); ## clear old one
	}elsif( $input == KEY_DOWN){
	    if (config_section_get_cur_index() < @config_section_list -1){
		config_section_set_cur_index(config_section_get_cur_index() + 1);
		display_entry_all();
		display_refresh($window_entry);
	    }
	    display_section(config_section_get_cur_index() -1 ); ## clear old one
	}
    }
    return $display_state;
}
###########################################
sub display_entry_edit(){
    my $entry_index = config_entry_get_cur_index();
    $window_grab = $window_entry;
    display_entry($entry_index);
    $display_state = $ST_ENTRY;
    display_entry_all();    ## clean up garbage
    return $display_state;
}

sub display_entry_select(){
    my $input;
    my @entry_list = config_entry_list();
    my $entry_index = config_entry_get_cur_index();

    $window_grab = $window_entry;
    display_entry($entry_index);
    $input = display_getch(0, $window_entry);
    if ( $input == KEY_LEFT){
	$display_state = $ST_SECT;
	display_entry( $entry_index);
	display_refresh($window_entry);
    }
    if ( $input == KEY_UP && $entry_index > 0){
	config_entry_set_cur_index( $entry_index -1);
	display_entry($entry_index);
    }
    if ( $input == KEY_DOWN && $entry_index < config_section_size() -1){
	    config_entry_set_cur_index( $entry_index + 1);
	    display_entry($entry_index );
	}
    if ( $input == '\n'){
	$display_state =  $ST_MODIFY;
    }
    return  $display_state;
}

sub display_entry(@){
    my $entry_index = shift;
    my $section = shift || config_section_get_cur_name();
    my $window = shift || $window_entry;
    config_entry_disp( $window, $section, $entry_index, $MARGIN_SIDE, $MARGIN_TOP + $entry_index * $ENTRY_SPAN);
}

sub display_entry_all(@){
    my $dest_window = shift || $window_entry;
    my $section_name =shift || config_section_get_cur_name();
    display_clear($dest_window);
    display_set_color($dest_window);
    box( $dest_window, ACS_VLINE, ACS_HLINE);
    $window_grab = $window_entry;

    my @entry_list = config_entry_list();
    my $index;
    foreach $index (0 .. @entry_list -1){
	display_entry($index);
    }
}

#########################################################
sub config_entry_sel_color(@){
## return the color for entry index
    my $entry_index = shift;
    my $section_name = shift || config_section_get_cur_name();
    unless(config_entry_is_cur($entry_index)){
	return $FG_COLOR;
    }elsif($display_state == $ST_ENTRY){
	return $EDIT_COLOR;
    }else{
	return $SELECT_COLOR;
    }
}

sub config_entry_data_color(@){
## return the color for entry contents
    my $entry_index = shift;
    my $section_name = shift || config_section_get_cur_name();
    unless(config_entry_is_cur($entry_index)){
	return $FG_COLOR;
    }elsif($display_state != $ST_ENTRY){
	return $SELECT_COLOR;
    }else{
	return $EDIT_COLOR;
    }
}

sub config_entry_is_cur(@){
## return the color for entry index
    my $entry_index = shift;
    my $section_name = shift || config_section_get_cur_name();
    return ($entry_index == config_entry_get_cur_index($section_name));
}

sub display_entry_text(){
    my $window = shift || $window_entry;
    my $entry = shift;
    my $x = shift;
    my $y = shift;
    my $title = "$$entry[$ENTRY_NAME]: ";

    display_str( $window, $title , $x, $y, 
		 config_entry_sel_color($$entry[$ENTRY_INDEX]));
    $x = tabpos( $x+ length($title));
    ### FixMe width 25 is too much (can be value of CTXT?)
    
    my ($maxx, $maxy);
    $window->getmaxyx($maxy, $maxx);
    $maxx = $maxx - $x - 1;

    if ($display_state == $ST_MODIFY){
	display_refresh($window_entry);  ## make sure color is changed
	 ### FixMe should support "cancel"
	
	return display_text_box( $window,
				 config_entry_get_data($entry),
				 $x, $y, $maxx,
				 $EDIT_COLOR,
			       );
    }else{
	display_text_box( $window,
			  config_entry_get_data($entry),
			  $x, $y, $maxx,
			  $FG_COLOR);
    }
    return $$entry[$ENTRY_DATA];
}

sub display_button(@){
    my $window = shift;
    my $data = shift;
    my $x = shift;
    my $y = shift;
    my $text_color = shift;
    my $side_color = shift;
    if ($side_color){
	display_str( $window, "<", $x , $y, $side_color);
    }else{
	display_str( $window, " ", $x , $y, $BG_COLOR);
    }
    $x += 1;
    display_str( $window, "$data ", $x , $y, $text_color);
    $x += length("$data");##ensure string
    display_str( $window, ">  ", $x, $y, $side_color) if ($side_color);
    return length($data) + 4;
}

sub display_entry_numeric(@){
    my $window = shift;
    my $entry = shift;
    my $x = shift;
    my $y = shift;
    my $input;
    my $min = $$entry[$ENTRY_CTXT][0];
    my $max = $$entry[$ENTRY_CTXT][1];
    my $step = $$entry[$ENTRY_CTXT][2];
    my $stateful = $$entry[$ENTRY_CTXT][3];
    my $title = "$$entry[$ENTRY_NAME]: ";
    my $value = config_entry_get_data($entry);
    my $errorval = $min -1; ## FixMe: KLUDGE
    display_str( $window, $title,
		 $x, $y, config_entry_sel_color($$entry[$ENTRY_INDEX]) );
    $x += 4 +length( $title);
    if ($stateful && $value == $errorval ){
	display_button( $window, "OFF", $x , $y, $FG_COLOR);
    }else{
	display_button( $window, $value, $x , $y, $FG_COLOR);
    }
    return $value unless ( $display_state == $ST_MODIFY);
    $value = $errorval if ($value eq "error");
#    display_set_color( $window, $FG_COLOR, $BG_COLOR);
    ### XXX FixMe: handle error(check for max,min,step)
    while(1){
	if ($value != $errorval){
	    display_button($window, $value , $x, $y, $FG_COLOR, $EDIT_COLOR);
	}else{
	    display_button($window, "OFF" , $x, $y, $FG_COLOR, $EDIT_COLOR);
	}
	$input = display_getch(0, $window);
	if ( $input == KEY_LEFT){
	    if ($value != $errorval){
		$value -= $step;
	    }else{
		$value = $max;
	    }
	}elsif ( $input == KEY_RIGHT){
	    if ($value != $errorval){
		$value += $step;
	    }else{
		$value = $min;
	    }
	}elsif ( $input == KEY_UP){
	    if ($value != $errorval){
		$value += $step * 2;
	    }else{
		$value = $min;
	    }
	}elsif ( $input == KEY_DOWN){
	    if ($value != $errorval){
		$value -= $step * 2;
	    }else{
		$value = $max;
	    }
	}elsif ( $input == '\n'){
	    last;
	}
	if ( $value < $min ){
	    if ( $stateful ){
		$value = $errorval ;
	    }else{
		$value = $max;
	    }
	}elsif ( $value > $max){
	    if ( $stateful ){
		$value = $errorval ;
	    }else{
		$value = $min;
	    }
	}
    }
    unless ($stateful){
	return $value; ## return the value from terminal emulator
    }else{
	if ($value == $errorval){
	    return "error";
	}else{
	    return $value;
	}
    }
}

sub display_entry_bool(){
## data is STRING whether "true" or "false"
    my $window = shift;
    my $entry = shift; 
    my $x = shift;
    my $y = shift;
    my $input;
    
    my $title = "$$entry[$ENTRY_NAME]: ";
    my $bool =  config_entry_get_data($entry);
    display_str( $window, $title,
		 $x, $y, config_entry_sel_color($$entry[$ENTRY_INDEX]) );
    $x = tabpos($x +length( $title));
    display_button( $window, $bool, $x, $y, $FG_COLOR);
    return $bool unless ( $display_state == $ST_MODIFY);
    while(1){
#	display_button();
	display_button($window, $bool , $x, $y, $FG_COLOR, $EDIT_COLOR);		
	$input = display_getch(0, $window);
	if ( $input == '\n'){
	    last;
	}elsif( $input != -1){ ### any char will change the state
	    $bool = ( $bool eq "true" ) ? "false" : "true";
	}
    }
    return  $bool; ## return the bool from terminal emulator
}

sub display_entry_list(){
######### FixMe IMPLEMENT THIS ROUTINE 
    my $parent = shift;
    my $entry = shift;
    my $x = shift;
    my $y = shift;

    my $input;
    my $array = $$entry[$ENTRY_CTXT];

    my $name = $$entry[$ENTRY_NAME];
    my $data = config_get_entry_data();
    display_str( $parent, "$name: ", $x, $y, config_entry_sel_color($$entry[$ENTRY_INDEX]) );
    $x += length( $name);
    display_str( $parent, $data, $x, $y, $FG_COLOR);
    return $data unless ( $display_state == $ST_MODIFY);

    my ($maxx, $maxy);
    $parent->getmaxyx($maxy,$maxx);
    if ($maxy -2 > @$array){
	$maxy = @$array + 2;
	$y -= @$array;
    }

    my $window	= derwin( $parent, 0, $x , $maxy - 2, $maxx- $x -2);

    while(1){
	display_button( $window, $data, $MARGIN_SIDE, $y, $SELECT_COLOR, $EDIT_COLOR);
	$input = display_getch(0, $window);
	if ( $input == '\n'){
	    last;
	}elsif( $input == KEY_UP){
	    
	}elsif( $input == KEY_UP){

	}elsif( $input == KEY_UP){

	}elsif( $input == KEY_UP){

	}elsif( $input == KEY_UP){

	}elsif( $input == KEY_UP){

	}elsif( $input == KEY_UP){

	}
    }
    return $data; ## return the bool from terminal emulator
}

sub display_entry_radio(){
### data is STRING for active button title
    my $window = shift;
    my $entry = shift;
    my $x = shift;
    my $y = shift;
    my $title = "$$entry[$ENTRY_NAME]: ";
    my $list = $$entry[$ENTRY_CTXT];
    my $i;
    my @xpos;
    my $input;
    my $index;
    my $value = config_entry_get_data($entry);

    display_str( $window, $title,
		 $x, $y, config_entry_sel_color($$entry[$ENTRY_INDEX]));
    $x = tabpos( $x  + length( $title)) ;
    ### XXX FixMe: stop to repeat creating pos/index information ...
    for( $i=0 ; $i < @$list ; $i++){
	$xpos[$i]=$x;
	if ( $$list[$i] eq $value){
	    $index = $i;
	    $x += display_button( $window, $$list[$i], $x , $y, $FG_COLOR, $SELECT_COLOR);
	}else{
	    $x += display_button( $window, $$list[$i], $x , $y, $FG_COLOR);
	}
    }
    return  $$list[$index] unless ( $display_state == $ST_MODIFY);

    while(1){
	display_button( $window, $$list[$index],
		     $xpos[$index] , $y, $SELECT_COLOR, $EDIT_COLOR);
	$input = display_getch(0, $window);
	if ( $input == KEY_LEFT){
	    display_button( $window, $$list[$index],
			    $xpos[$index] , $y, $FG_COLOR);
	    $index --;
	    $index = @xpos -1 if ( $index < 0 );
	}elsif ( $input == KEY_RIGHT){
	    display_button( $window, $$list[$index],
			    $xpos[$index] , $y, $FG_COLOR);
	    $index ++;
	    $index = 0 if ( $index >= @xpos );
	}elsif ( $input == '\n'){
	    last;
	}
    }
    return $$list[$index];
}

sub display_entry_color(@){
## value is color name
    my $window = shift;
    my $entry = shift;
    my $x = shift;
    my $y = shift;

    my $title = "$$entry[$ENTRY_NAME]: ";
    my $colorname =  config_entry_get_data($entry);
    display_str( $window, $title,
		 $x, $y, config_entry_sel_color($$entry[$ENTRY_INDEX]));
    $x = tabpos ($x + length( $title));
    display_str( $window, $colorname,
		$x, $y, $colorname);
    display_str( $window, "($colorname)",
		$x +  2 + length( $colorname), $y,
		$FG_COLOR);
    return $colorname if ( $display_state != $ST_MODIFY);
    $colorname = display_color_selector( $window, $x, $y + 1, $colorname);
    return $colorname;
}

sub display_entry_system(@){
    my $window = shift;
    my $entry = shift;
    my $x = shift;
    my $y = shift;
    my $rightside = shift || 0;
    my ( $maxx, $maxy);
    my $title = "$$entry[$ENTRY_NAME]";
    my $msg = $$entry[$ENTRY_CTXT];
    my ( $key , $result);
    my $choice = 0;

    $window->getmaxyx($maxy,$maxx);
    unless ($rightside){
	display_str( $window, $title, $MARGIN_SIDE , $y,
		     config_entry_sel_color($$entry[$ENTRY_INDEX]));
    }else{
	display_str( $window, $title, $maxx-length($title) -$MARGIN_SIDE, $y,
		     config_entry_sel_color($$entry[$ENTRY_INDEX]));
    }
    if ( $display_state == $ST_MODIFY){
	my $left = $$msg[1];
	my $right = $$msg[2];
	my $data = sprintf("%-16s", $$msg[0]);
	my $window_dlg = newwin(5, length( $data) + 4,
				( $LINES-5)/2, ( $COLS-length( $data))/2 -2 );
	$window_dlg->keypad(1);
	display_set_color( $window_dlg, $FG_COLOR, $BG_COLOR);
	display_clear($window_dlg);
	box( $window_dlg, ACS_VLINE, ACS_HLINE);	
	my $input = 1;
	while(1){
	    display_str( $window_dlg, $data, 2, 1);
	    display_str( $window_dlg, $left, 1, 3,
			(( $choice) ? $FG_COLOR : $SELECT_COLOR));
	    display_str( $window_dlg, $right, length( $data) -length( $right) , 3,
			(( $choice) ? $SELECT_COLOR : $FG_COLOR));
	    $input = display_getch(0, $window_dlg);
	    if ( $input == KEY_LEFT){
		$choice = 1 - $choice;
	    }elsif ( $input == KEY_RIGHT){
		$choice = 1 - $choice;
	    }elsif ( $input == '\n'){
		last;
	    }
	}
	$window_dlg->delwin;
    }
    return $choice;
}

sub display_text_box(@){
    my $parent = shift;
    my $text = shift; ## to display
    my $x = shift;
    my $y = shift;
    my $width = shift;
    my $color = shift;

    my $window_dlg;
    my $pos = 0;
    my $input;
    my $offset = 0;

    $window_dlg= derwin( $parent, $MARGIN_SIDE, $width, $y, $x);
    display_set_color( $window_dlg, $color, $BG_COLOR);
    display_clear($window_dlg);
    $window_dlg->keypad(1);
    while(1){
	$window_dlg->addstr(0, 0, substr($text, $offset, $width -1) );
	if ( $display_state == $ST_MODIFY && ($pos-$offset) < $width){
	    $window_dlg->attron(A_BOLD);
	    $window_dlg->addstr(0, $pos - $offset  , substr( $text, $pos ,1));
	    $window_dlg->attroff(A_BOLD);
	}
	$window_dlg->addstr(0, length( $text) - $offset, " ");
	if ( $display_state != $ST_MODIFY){
	    $window_dlg->delwin;
	    return $text;
	}
	$input = display_getch(0, $window_dlg);
	if ( $input =~ /^[ A-Za-z\/\"_0-9,.\%:;]$/) {
	    substr( $text, $pos , 0) = $input ;
	    $pos++;
	    next;
	}
	if ( $input eq "\n") {
	    $window_dlg->delwin;
	    return ( $input, $text);
	} elsif ( $input eq KEY_BACKSPACE ||$input eq pack("c",127) ) {
	    if ( $pos >0){
		substr( $text, $pos - 1 , 1) = '' ;
		$pos --;
	    }else{
		substr( $text, 0 , 1) = '' 
	    }	
	} elsif ( $input eq KEY_LEFT) {
	    $pos-- if ( $pos > 0);
	    $offset = $pos if ($pos < $offset);
	} elsif ( $input eq KEY_RIGHT) {
	    if ( $pos < length( $text) ){
		if ( $pos - $offset > $width  ){
		    $offset++;
		}
		$pos++;
	    }
	} elsif ( $input eq KEY_HOME) {
	    $pos = 0;
	    $offset = 0;
	} elsif ( $input eq KEY_END) {
	    $pos = length( $text) ;
	    if ( length ($text) > $width -1){
		$offset = length($text) - $width +1;
	    }else{
		$offset = 0;
	    }
	} elsif ( $input =~ /\t/) {
	    ### XXX autocomplete?
	}
    }
}

sub display_color_selector(@){
    my $window = shift;
    my $x = shift;
    my $y = shift;
    my $current = shift;
    my $i;
    my $color;
    my ($maxx,$maxy);
    $window->getmaxyx($maxy, $maxx);
    $i = $y + @color_table  +3 - $maxy ;
    $y -= $i  if ($i > 0);  ### FixMe also handle under flow
    my $window_dlg = derwin( $window, @color_table +2, 30,
			    $y , $x );
    $window_dlg->keypad(1);
    display_set_color( $window_dlg, $FG_COLOR, $BG_COLOR);
    display_clear($window_dlg);
    box( $window_dlg, ACS_VLINE, ACS_HLINE);	
    my $input = 1;
    for ( $i = 0; $color_table[$i] ; $i++){
	$current = $i if ( $current eq $color_table[$i]);
    }
    while(1){
	for ( $i = 0; $i <@color_table  ; $i++){
	    display_str( $window_dlg, (( $i == $current)? ">":" ") ,
			1, $i + 1, $FG_COLOR);
	    display_str( $window_dlg, "<>" ,
			2, $i + 1, $color_table_map[$i] ,$color_table_map[$i]);
	    display_str( $window_dlg, "( $color_table[$i])" ,
			5, $i + 1, $FG_COLOR);
	}
	
	$input= display_getch(0, $window_dlg);
	if ( $input == KEY_LEFT || $input == KEY_UP){
	    $current -=1 if ( $current >0);
	}elsif ( $input == KEY_RIGHT || $input == KEY_DOWN){
	    $current +=1 if ( $current < @color_table -1);
	}elsif ( $input == '\n'){
	    last;
	}
    }
    $window_dlg->delwin;
    display_section(config_section_get_cur_index());
    return $color_table[$current];
}

sub display_entry_sys_discard(@){
    if (display_entry_system(@_) == 1){
	display_final();
	exit(1);	
    }
    return 1; ##dummy
}

sub display_entry_sys_finish(@){
    if (display_entry_system(@_) == 1){
	display_final();
	config_apply();
	exit(0);	
    }
    return 1; ##dummy
}

sub display_entry_sys_apply(@){
    if (display_entry_system(@_) == 1){
	$display_state = 0;###  XXX sometimes fails...
	display_final();
	config_apply_all();
	display_init();	
	display_section_all();
	display_refresh($window_sect);
	
	display_entry_all();
    }
    return 1; ##dummy
}

sub display_entry_sys_apply_section(@){
    if (display_entry_system(@_,1) == 1){
	$display_state = 0;###  XXX sometimes fails...
	display_final();
	config_apply_section();
	display_init();	
	display_section_all(); ### current is used
	display_refresh($window_sect);
	display_entry_all();
    }
    return 1; ##dummy
}

sub display_entry_sys_revert_section(@){
    if (display_entry_system(@_,1) == 1){
	config_revert_section();
    }
    return 1; ##dummy
}

sub display_getch(@) {
    my $nonblock = shift;
    my $window = shift || $window_grab;

    my $key = -1;
    if ($nonblock){
	halfdelay(5); ## XXX FixMe: try cbreak ?
    }else{
	cbreak();
    }
    $key = $window->getch();

    halfdelay(5);
    return $key;
}

#############################################
# communication with terminal XXX FixMe:implement general esc. seq.
###########################################
my %entry2key_mlterm = ### XXX FixMe: combine %config_tree structure?
(
 'Encoding' => 'encoding',
 'XIM       ' => 'xim',
 'XIM_locale' => 'locale',
 'process via UCS' => 'copy_paste_via_ucs',
 'Wallpaper' => 'wall_picture',
 'Foreground' => 'fg_color',
 'Background' => 'bg_color',
 'Font Size' => 'fontsize',
 'Fade Ratio' => 'fade_ratio',
 'Variable Width' => 'use_variable_column_width',
 'Anti Alias' => 'use_anti_alias',
 'Transparent'=>'use_transbg',
 'Tab Size'=>'tabsize',
 'Log Size'=>'logsize',
 'Mod Meta Mode'=>'mod_meta_mode',
 'Bel Mode'=>'bel_mode',
 'Combining'=>'use_combining',
 'BiDi'=>'use_bidi',
);

sub comm_dummy(){
    #
}

sub comm_getparam_mlterm(@){
### XXX FixMe: distinguish 0 from "error" or undef correctly
    my $entry = shift;
    my ( $key, $ret);
    my $input = 0;
    $key = $entry2key_mlterm{$entry};
    if ( $key){
	
	printf "\x1b]5380;${key}\x07";
	### FixMe: support /dev/pts/n for remote control
	while( $input ne '#'){
	    $input = display_getch(1);
	    goto ERR if ( $input == -1);
	}
	while(( $input = display_getch(1)) ne '='){
	    goto ERR if ( $input == -1);
	    $ret .= $input;
	    goto ERR if ( $ret eq "error"); ## FixMe handle error
	    goto ERR if ( $input eq "\n"); ## FixMe handle error
	}
	unless ( $ret eq $key){
	    goto ERR;
	}
	$ret = "";
	while(( $input = display_getch(1)) ne "\n"){
	    last if ( $input == -1);	
	    $ret .= $input;
	}
	return  $ret;
    }
 ERR:
    return $MAGIC_ERROR;
}

sub comm_setparam_mlterm(@){
    my $entry = shift;
    my $data = shift;
    my $key = $entry2key_mlterm{$entry};
    if ( $key){
###FixMe: 
#	open (OUT,">$TTY");
#	warn "tty $TTY";
#	printf OUT "\x1b]5379;${key}=${data}\x07";
#	close(OUT);
	printf "\x1b]5379;${key}=${data}\x07";
    }
}

sub comm_getparam_internal(@){
    my $entry = shift;
    return $TTY if ( $entry eq "pty");
    return $FG_COLOR if ( $entry eq "foreground");
    return $BG_COLOR if ( $entry eq "background");
    return $EDIT_COLOR if ( $entry eq "editing");
    return $SELECT_COLOR if ( $entry eq "selection");
    return "error";
}

sub comm_setparam_internal(@){
    my $entry = shift;
    my $data = shift;
    $TTY = $data if ( $entry eq "pty");
    $FG_COLOR = $data if ( $entry eq "foreground");
    $BG_COLOR = $data if ( $entry eq "background");
    $EDIT_COLOR = $data if ( $entry eq "editing");
    $SELECT_COLOR = $data if ( $entry eq "selection");
}

#####################################################
#  setup data structure  XXX FixMe: separate to another file
####################################################

config_section_add(
		   "Encoding",
		   "Cut & Paste",
		   "Appearance",
		   "Others",
		   "Configuration",
	       );

config_entry_add_from_list(
		      ["Encoding" , #section to add
		       "Encoding" , #entry name
		       \&comm_getparam_mlterm , #func to get
		       \&comm_setparam_mlterm , #func to set
		       \&display_entry_text ,   #func to display
		       ['UTF8',     #data for display
                        'ISO88591',
                        'ISO88592',
                        'ISO88593',
                        'ISO88594',
                        'ISO88595',
                        'ISO88596',
                        'ISO88597',
                        'ISO88598',
                        'ISO88599',
                        'ISO885910',
                        'ISO885911',
                        'ISO885913',
                        'ISO885914',
                        'ISO885915',
                        'ISO885916',
                        'TCVN5712',
                        'ISCII',
                        'VISCII',
                        'KOI8R',
                        'KOI8U',
                        'EUCJP',
                        'EUCJISX0213',
                        'SJIS',
                        'SJISX0213',
                        'ISO2022JP',
                        'ISO2022JP2',
                        'ISO2022JP3',
                        'EUCKR',
                        'UHC',
                        'JOHAB',
                        'ISO2022KR',
                        'EUCTW',
                        'BIG5',
                        'BIG5HKSCS',
                        'EUCCN',
                        'GBK',
                        'GB18030',
                        'HZ',
                        'ISO2022CN']
		       ],
##### not yet implemented?
#		      ["Encoding" ,           
#		       "ISCII lang" ,
#		       \&comm_getparam_mlterm ,
#		       \&comm_setparam_mlterm ,
#		       \&display_entry_text ,
#		       undef
#		      ],
		      ["Encoding" ,
		       "XIM       " ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_text ,
		       ["_XWNMO", "kinput2"] ],
		      ["Encoding" ,
		       "XIM_locale" , 
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_text ,
		       ["ja_JP.eucJP", "ja_JP.UTF-8"] ],
		      ["Encoding" ,
		       "BiDi" , 
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Encoding" ,
		       "Combining" , 
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Encoding" ,
		       "[Revert change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_revert_section ,
		       ["Revert changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Encoding" ,
		       "[Apply change]" ,
		       \&comm_dummy ,  \&comm_dummy ,
		       \&display_entry_sys_apply_section ,
		       ["Apply changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Cut & Paste" ,
		       "process via UCS" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Cut & Paste" ,
		       "[Revert change]" ,
		       \&comm_dummy ,\&comm_dummy ,
		       \&display_entry_sys_revert_section ,
		       ["Revert changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Cut & Paste" ,
		       "[Apply change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_apply_section ,
		       ["Apply changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Appearance" ,
		       "Foreground" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_color ,
		       undef ],
		      ["Appearance" ,
		       "Background" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_color ,
		       undef ],
		      ["Appearance" ,
		       "Font Size" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_numeric ,
		       [8, 48, 1, 0] ],### XXX needs max_font_size?
		      ["Appearance" ,
		       "Fade Ratio" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_numeric ,
		       [0, 100, 1, 1] ], ## XXX 10% step should be enough ?
		      ["Appearance" ,
		       "Variable Width" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Appearance" ,
		       "Anti Alias" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Appearance" ,
		       "Transparent" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_bool ,
		       undef ],
		      ["Appearance" ,
		       "Wallpaper" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_text ,
		       undef ],
		      ["Appearance" ,
		       "[Revert change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_revert_section ,
		       ["Revert changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Appearance" ,
		       "[Apply change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_apply_section ,
		       ["Apply changes to this section?" , "<Cancel>", "<OK>"] ],
		       ["Others" ,
			"Tab Size" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_numeric ,
		       [2, 16, 1, 0] ],### XXX ?
		       ["Others" ,
			"Log Size" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_numeric ,
		       [128, 1024, 128, 0] ], ## XXX support _logarithmic?
		       ["Others" ,
			"Mod Meta Mode" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_radio ,
		       ["none", "esc", "8bit"] ],
		       ["Others" ,
			"Bel Mode" ,
		       \&comm_getparam_mlterm ,
		       \&comm_setparam_mlterm ,
		       \&display_entry_radio ,
			["none", "sound", "visual"] ],
		      ["Others" ,
		       "[Revert change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_revert_section ,
		       ["Revert changes to this section?" , "<Cancel>", "<OK>"] ],
		      ["Others" ,
		       "[Apply change]" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_apply_section ,
		       ["Apply changes to this section?" , "<Cancel>", "<OK>"] ],
		       ["Configuration" ,
			"Quit (apply changes)" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_finish ,
		       ["Really quit and change terminal settings?", "<Cancel>", "<OK>"] ],
		       ["Configuration" ,
			"Quit (discard changes)" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_discard ,
		       ["Really quit discarding all changes?", "<Cancel>", "<OK>"] ],
		      ["Configuration" ,
		       "Apply all changes" ,
		       \&comm_dummy , \&comm_dummy ,
		       \&display_entry_sys_apply ,
		       ["Really apply all changes?", "<Cancel>", "<OK>"] ],
####not yet supported
#		      ["Configuration" ,
#		       "pty" ,
#		       \&comm_getparam_internal ,
#		       \&comm_setparam_internal ,
#		       \&display_entry_text ,
#		       undef ],
		      ["Configuration" ,
		       "foreground" ,
		       \&comm_getparam_internal ,
		       \&comm_setparam_internal ,
		       \&display_entry_color,
		       undef ],
		      ["Configuration" ,
		       "background" ,
		       \&comm_getparam_internal ,
		       \&comm_setparam_internal ,
		       \&display_entry_color ,
		       undef ],
		      ["Configuration" ,
		       "selection" ,
		       \&comm_getparam_internal ,
		       \&comm_setparam_internal ,
		       \&display_entry_color ,
		       undef],
		      ["Configuration" ,
		       "editing" ,
		       \&comm_getparam_internal ,
		       \&comm_setparam_internal ,
		       \&display_entry_color ,
		       undef ]
		     );

### tty selection is not working yet #################################
#if (open(IN,"/usr/bin/tty|")){
#    $TTY=scalar<IN>;
#    close(IN);
#}



display_init();


display_refresh($window_sect);
#config_init_fg(); ## getch needs a curses window.
display_entry_all();
#config_init_bg(); ## getch needs a curses window.
display_section_all();
#display_entry_all();
display_refresh($window_entry);

while ($display_state >= 0){
    $display_state = display_main();
}

display_final();


