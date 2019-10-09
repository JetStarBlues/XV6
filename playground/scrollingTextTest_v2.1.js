// By @acedean
// khanacademy.org/cs/a/5465018136756224

/* If use getc to create buffer, wrap code is
   simple (because per char) while noWrap is complex.
   If use getline to create buffer, opposite is true.
*/

var txt = "Birds, also known as Aves or avian dinosaurs, are a group of endothermic vertebrates, characterised by feathers, toothless beaked jaws, the laying of hard-shelled eggs, a high metabolic rate, a four-chambered heart, and a strong yet lightweight skeleton.\nSmall line.\nDouble line.\n\n\nBirds live worldwide and range in size from the 5 cm (2 in) bee hummingbird to the 2.75 m (9 ft) ostrich.\nThey rank as the world's most numerically successful class of tetrapods, with approximately ten thousand living species, more than half of these being passerine, or \"perching\" birds.\nBirds have wings whose development varies according to species; the only known groups without wings are the extinct moa and elephant birds.\nWings, which evolved from forelimbs, gave birds the ability to fly, although further evolution has led to the loss of flight in some birds, including ratites, penguins, and diverse endemic island species of birds.\nThe digestive and respiratory systems of birds are also uniquely adapted for flight.\nSome bird species of aquatic environments, particularly seabirds and some waterbirds, have further evolved for swimming.\nThe fossil record demonstrates that birds are modern feathered dinosaurs, having evolved from earlier feathered dinosaurs within the theropod group, which are traditionally placed within the saurischian dinosaurs.\nThe closest living relatives of birds are the crocodilians.\nPrimitive bird-like dinosaurs that lie outside class Aves proper, in the broader group Avialae, have been found dating back to the mid-Jurassic period, around 170 million years ago.\nMany of these early \"stem-birds\", such as Archaeopteryx, retained primitive characteristics such as teeth and long bony tails.\nDNA-based evidence finds that birds diversified dramatically around the time of the Cretaceous–Palaeogene extinction event 66 million years ago, which killed off the pterosaurs and all the non-avian dinosaur lineages.\nBut birds, especially those in the southern continents, survived this event and then migrated to other parts of the world while diversifying during periods of global cooling.\nThis makes them the sole surviving dinosaurs according to cladistics.\nSome birds, especially corvids and parrots, are among the most intelligent animals; several bird species make and use tools, and many social species pass on knowledge across generations, which is considered a form of culture.\nMany species migrate annually over great distances. Birds are social, communicating with visual signals, calls, and bird songs, and participating in such social behaviours as cooperative breeding and hunting, flocking, and mobbing of predators.\nThe vast majority of bird species are socially monogamous (referring to social living arrangement, distinct from genetic monogamy), usually for one breeding season at a time, sometimes for years, but rarely for life.\nOther species have breeding systems that are polygynous (arrangement of one male with many females) or, rarely, polyandrous (arrangement of one female with many males).\nBirds produce offspring by laying eggs which are fertilised through sexual reproduction.\nThey are usually laid in a nest and incubated by the parents.\nMost birds have an extended period of parental care after hatching.\nSome birds, such as hens, lay eggs even when not fertilised, which do not produce offspring.\nMany species of birds are economically important as food for human consumption and raw material in manufacturing, with domesticated and undomesticated birds (poultry and game) being important sources of eggs, meat, and feathers.\nSongbirds, parrots, and other species are popular as pets.\nGuano (bird excrement) is harvested for use as a fertiliser.\nBirds figure throughout human culture.\nAbout 120 to 130 species have become extinct due to human activity since the 17th century, and hundreds more before then.\nHuman activity threatens about 1,200 bird species with extinction, though efforts are underway to protect them.\nRecreational birdwatching is an important part of the ecotourism industry.\n";

// Array of lines
var txtBuffer = [];
var maxLineLen = 0;
var readLines = function ()
{
    var idx = 0;
    
    // while ( getChar() )
    while ( idx < txt.length )
    {
        var line = [];
        
        while ( txt.charAt( idx ) !== '\n' )
        {
            line.push( txt.charAt( idx ) );
            
            idx += 1;
        }
        
        idx += 1;  // skip newline
        
        // println( "\n$ " + line );
        
        txtBuffer.push( line );
        
        if ( line.length > maxLineLen )
        {
            maxLineLen = line.length;
            
            // println( line );
        }
    }
};

readLines();


// var font = createFont( "monospace" );
// var font = createFont( "PICO-8" );
var font = createFont( "GohuFont" );
textFont( font );

var fHeight = 12;
var fWidth  = fHeight;
textSize( fHeight );
textAlign( LEFT, TOP );

var lineSpacing = 2;
var tHeight = fHeight + lineSpacing;
var tWidth = fWidth;

var scWidth  = 315;
var scHeight = 300;

var charsPerRow = floor( scWidth / tWidth );
var nVisibleRows = floor( scHeight / tHeight );
var nVisibleChars = nVisibleRows * charsPerRow;

maxLineLen = max( maxLineLen, charsPerRow );
// println( maxLineLen );


var wrapText = 1;

var scrollYOffset = max( 0, -2 );
var scrollXOffset = max( 0, 5 );


/* Get underlying buffer position of a char
   during "wrapText" mode
*/
var getWrappedCharBufferPos = function ( cX, cY )
{
    // If wrapping, no scrollXOffset...

    // if ( wrapText === 0 ) { return null; }

    var lineIdx = 0;
    var charIdx = cX;
    
    var vLineIdx  = 0;  // "virtual/visual"
    var vLineIdx2 = 0;
    var vLineIdxTarget = cY + scrollYOffset;

    var linne;
    
    while ( vLineIdx < vLineIdxTarget )
    {
        if ( lineIdx >= txtBuffer.length ) { return; }

        linne = txtBuffer[ lineIdx ];
        
        var lineLen = linne.length > 0 ? linne.length : 1;
        
        vLineIdx2 = vLineIdx + ceil( lineLen / charsPerRow );
        
        if ( vLineIdx2 === vLineIdxTarget )
        {
            lineIdx += 1;  // consume entire line

            break;  // done
        }
        else if ( vLineIdx2 > vLineIdxTarget )
        {
            // consume partial line
            charIdx += ( vLineIdxTarget - vLineIdx ) * charsPerRow;
            
            break;  // done
        }
        else if ( vLineIdx2 < vLineIdxTarget )
        {
            lineIdx += 1;  // consume entire line
            
            vLineIdx = vLineIdx2;
        }
    }
    
    return {
        
        "lineIdx" : lineIdx,
        "charIdx" : charIdx
    };
};


var drawNoWrap = function ()
{
    var x = 0;
    var y = 0;
    
    var nLinesDrawn = 0;
    
    var lineIdx = scrollYOffset;  //
    
    // Per line
    while ( true )
    {
        if ( nLinesDrawn === nVisibleRows ) { break; }
        
        if ( lineIdx >= txtBuffer.length ) { break; }
    
        var linne = txtBuffer[ lineIdx ];

        var charIdx = scrollXOffset;  //

        var nCharsDrawn = 0;
        
        // Per char
        while ( nCharsDrawn < charsPerRow )
        {
            if ( charIdx >= linne.length ) { break; }
            
            text( linne[ charIdx ], x, y );
            
            charIdx += 1;
            nCharsDrawn += 1;
            
            x += tWidth;
        }
        
        lineIdx += 1;
        nLinesDrawn += 1;
        
        y += tHeight;
        x = 0;
    }
};

var drawWrap = function ()
{
    /* Find starting lineIdx and charIdx...
       Has to be computed as visual/fake lines
       generated by wrapping unknown
    */
    var startPos = getWrappedCharBufferPos( 0, 0 );
    var lineIdx  = startPos.lineIdx;
    var charIdx  = startPos.charIdx;

    var x = 0;
    var y = 0;
    
    var nLinesDrawn = 0;    
    
    var first = true;  // Hacky =P
    
    
    // Per line
    while ( true )
    {
        if ( nLinesDrawn >= nVisibleRows ) { break; }
        
        if ( lineIdx >= txtBuffer.length ) { break; }
    
        var linne = txtBuffer[ lineIdx ];

        if ( first )
        {
            first = false;  // Use charIdx associated with scrollY
        }
        else
        {
            charIdx = 0;  // Start at first char of line
        }
        
        // Per char
        while ( charIdx < linne.length )
        {
            text( linne[ charIdx ], x, y );
            
            charIdx += 1;
            
            x += tWidth;

            if ( ( charIdx % charsPerRow ) === 0 )
            {
                y += tHeight;
                x = 0;
                
                nLinesDrawn += 1;
                
                if ( nLinesDrawn >= nVisibleRows ) { break; }
            }
        }
        
        // println( lineIdx );
        
        lineIdx += 1;
        nLinesDrawn += 1;
        
        y += tHeight;
        x = 0;
    }
};

var update = function ()
{
    // Clear background
    noStroke();
    fill(255, 255, 255);
    rect( 0, 0, width, height );
    
    // Draw "screen" bounds
    stroke(45, 117, 49);
    line( scWidth, 0, scWidth, scHeight );
    line( 0, scHeight, scWidth, scHeight );
    noStroke();

    // Draw text
    fill(255, 0, 0);
    
    if ( wrapText )
    {
        drawWrap();
    }
    else
    {
        drawNoWrap();
    }
};


var drawInversion = function ( c, x, y )
{
    fill(255, 0, 0);
    rect( x, y, tWidth, tHeight );
    fill(255, 255, 255);
    text( c, x, y );
};

var getSelectedChar = function ( mX, mY )
{
    var lineIdx;
    var charIdx;
    var char;
    
    var cX = floor( mX / tWidth );
    var cY = floor( mY / tHeight );
    
    var x = cX * tWidth;
    var y = cY * tHeight;

    if ( wrapText )
    {
        var charPos = getWrappedCharBufferPos( cX, cY );
    
        lineIdx = charPos.lineIdx;
        charIdx = charPos.charIdx;
    }

    else
    {
        lineIdx = cY;
        lineIdx += scrollYOffset;
        
        if ( lineIdx >= txtBuffer.length ) { return; }
        
        charIdx = cX;
        charIdx += scrollXOffset;
        
    }
    
    if ( charIdx >= txtBuffer[ lineIdx ].length )
    {
        // return;  // "unselectable"

        char = "_";
    }
    else
    {
        char = txtBuffer[ lineIdx ][ charIdx ];
    }

    update();
    drawInversion( char, x, y );
};

var draw = function ()
{
    update();

    noLoop();
};

var mouseClicked = function ()
{
    getSelectedChar( mouseX, mouseY );
};

